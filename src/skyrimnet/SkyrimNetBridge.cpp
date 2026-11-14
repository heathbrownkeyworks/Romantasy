#include "skyrimnet/SkyrimNetBridge.h"

#include "settings/Settings.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <array>
#include <memory>

namespace
{
    // Declarations copied from SkyrimNet's plugin API header, sdk/cpp/PublicAPI.h in
    // github.com/MinLL/SkyrimNet-ExamplePlugin (SDK 0.29.0, API version 14; MIT, Copyright (c) 2025-2026 Fateless.Ai).
    // Only the two this build calls, and only those that pass plain C values. The header declares them in an
    // extern "C" block as plain function pointers (the x64 calling convention; none is named).
    //
    //   inline int (*PublicGetVersion)() = nullptr;                                         // v2
    //   inline int (*PublicRegisterEvent)(const char* eventType, const char* content, uint32_t originatorFormId,
    //       uint32_t targetFormId, const uint32_t* audienceFormIds, uint32_t audienceCount) = nullptr;  // v11
    using GetVersionFn = int (*)();
    using RegisterEventFn = int (*)(const char* eventType, const char* content, std::uint32_t originatorFormId,
        std::uint32_t targetFormId, const std::uint32_t* audienceFormIds, std::uint32_t audienceCount);

    // PublicRegisterEvent arrived in API v11.
    constexpr int kMinimumApiVersion = 11;
    // Plain text wrapped as {"line": ...} (Papyrus RegisterPersistentEvent). Sent with no originator and the follower as
    // target and only audience, it is narration the follower sees in their history; SkyrimNet may have them react to
    // it in their next line. Other NPCs do not see it.
    constexpr const char* kEventType = "persistent_generic";

    // Set once by Start on the game thread before any event is sent; read by SendEvent on the game thread.
    std::atomic<RegisterEventFn> g_registerEvent = nullptr;
    // SendEvent says once per session that SkyrimNet stored nothing for an event.
    std::atomic_bool g_nothingStoredLogged = false;

    // The bond line goes through SkyrimNet's Papyrus API (SkyrimNetApi.psc, MIT): only Papyrus values cross, so the
    // two C runtimes never share an object. Romantasy_SkyrimNet.BondDecorator(Actor) Global returns the line.
    constexpr const char* kApiScript = "SkyrimNetApi";
    constexpr const char* kRegisterDecorator = "RegisterDecorator";
    constexpr const char* kDecoratorName = "romantasy_bond";
    constexpr const char* kDecoratorScript = "Romantasy_SkyrimNet";
    constexpr const char* kDecoratorFunction = "BondDecorator";

    // The conversation actions (SkyrimNetApi.RegisterAction): SkyrimNet asks Romantasy_SkyrimNet's eligibility
    // function whether the actor may take one, and runs its execution function when the actor does. The descriptions
    // are fixed text: SkyrimNet renders them as templates, so nothing from a profile, a save or a name goes in.
    constexpr const char* kRegisterAction = "RegisterAction";
    constexpr const char* kActionCategory = "PAPYRUS";
    constexpr std::int32_t kActionPriority = 1;
    struct Action
    {
        const char* name;
        const char* description;
        const char* eligibility;
        const char* execution;
    };
    constexpr std::array<Action, 2> kActions{ {
        { "RomantasyTouched",
            "You are genuinely moved by something kind, thoughtful or loving {{ player.name }} just said or did. Use rarely, only for moments that truly matter to you.",
            "TouchedEligible", "TouchedExecute" },
        { "RomantasyHurt",
            "You are genuinely hurt or angered by something cruel, dismissive or disloyal {{ player.name }} just said or did. Use rarely.",
            "HurtEligible", "HurtExecute" },
    } };

    [[nodiscard]] std::optional<std::int32_t> IntAnswer(const RE::BSScript::Variable& result)
    {
        return result.IsInt() ? std::optional<std::int32_t>(result.GetSInt()) : std::nullopt;
    }

    [[nodiscard]] std::string Raw(std::optional<std::int32_t> result)
    {
        return result ? std::to_string(*result) : std::string("not a number");
    }

    // Receives RegisterDecorator's int on the VM thread.
    class DecoratorResult final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        explicit DecoratorResult(std::uint32_t stamp) : _stamp(stamp) {}
        void operator()(RE::BSScript::Variable a_result) override
        {
            SkyrimNetBridge::GetSingleton().OnDecoratorResult(_stamp, IntAnswer(a_result));
        }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        std::uint32_t _stamp;
    };

    // Receives RegisterAction's int on the VM thread. `action` is one of the static names above.
    class ActionResult final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        ActionResult(std::uint32_t stamp, const char* action) : _stamp(stamp), _action(action) {}
        void operator()(RE::BSScript::Variable a_result) override
        {
            SkyrimNetBridge::GetSingleton().OnActionResult(_stamp, _action, IntAnswer(a_result));
        }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        std::uint32_t _stamp;
        const char* _action;
    };
}

SkyrimNetBridge& SkyrimNetBridge::GetSingleton()
{
    static SkyrimNetBridge singleton;
    return singleton;
}

void SkyrimNetBridge::Start()
{
    if (_started.exchange(true)) {
        return;
    }
    // Only a module already loaded: Romantasy must never be the reason SkyrimNet loads. SKSE has loaded every plugin
    // by data-loaded.
    const HMODULE module = GetModuleHandleW(L"SkyrimNet.dll");
    if (!module) {
        logger::info("SkyrimNet not found; the bridge is off.");
        return;
    }
    const auto getVersion = reinterpret_cast<GetVersionFn>(GetProcAddress(module, "PublicGetVersion"));
    const int version = getVersion ? getVersion() : 0;
    _apiVersion = version > 0 ? static_cast<std::uint32_t>(version) : 0u;
    if (version < kMinimumApiVersion) {
        _tooOld = true;
        logger::warn("SkyrimNet found, but its API version ({}) is older than the bridge needs ({}); the bridge is off.",
            version, kMinimumApiVersion);
        return;
    }
    const auto registerEvent = reinterpret_cast<RegisterEventFn>(GetProcAddress(module, "PublicRegisterEvent"));
    if (!registerEvent) {
        _tooOld = true;
        logger::warn("SkyrimNet found (API v{}), but it has no PublicRegisterEvent; the bridge is off.", version);
        return;
    }
    g_registerEvent = registerEvent;
    _found = true;
    logger::info("SkyrimNet found (API v{}); the bridge is {}. Events are sent; the bond decorator and the conversation actions are registered through Papyrus after a save loads.",
        version, Settings::GetSingleton().SkyrimNetBridge() ? "on" : "off");
}

void SkyrimNetBridge::RegisterDecorator()
{
    // Found or too old: the Papyrus registrations need no C export, and the shipped prompt file calls romantasy_bond
    // either way. With a SkyrimNet too old the natives answer "" and false, so it gets a function that returns nothing.
    if (!Found() && !TooOld()) {
        return;
    }
    const auto stamp = _load.fetch_add(1) + 1;
    _decorator = Decorator::Pending;
    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        _decorator = Decorator::Failed;
        logger::warn("SkyrimNet: the Papyrus VM is not ready, so the bond line is not registered for this load.");
    } else {
        // The VM copies the arguments while it dispatches; they are ours to free afterwards.
        const std::unique_ptr<RE::BSScript::IFunctionArguments> args(RE::MakeFunctionArguments(
            RE::BSFixedString(kDecoratorName), RE::BSFixedString(kDecoratorScript), RE::BSFixedString(kDecoratorFunction)));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new DecoratorResult(stamp));
        if (!vm->DispatchStaticCall(kApiScript, kRegisterDecorator, args.get(), callback)) {
            _decorator = Decorator::Failed;
            logger::warn("SkyrimNet: could not call SkyrimNetApi.RegisterDecorator, so the bond line is not registered for this load.");
        }
    }
    RegisterActions();
}

void SkyrimNetBridge::RegisterActions()
{
    if (!Found() && !TooOld()) {
        return;
    }
    const auto stamp = _load.load();
    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        logger::warn("SkyrimNet: the Papyrus VM is not ready, so conversation cannot move bonds for this load.");
        return;
    }
    for (const auto& action : kActions) {
        // All twelve arguments: a dispatch through the VM fills no Papyrus defaults.
        const std::unique_ptr<RE::BSScript::IFunctionArguments> args(RE::MakeFunctionArguments(
            RE::BSFixedString(action.name), RE::BSFixedString(action.description),
            RE::BSFixedString(kDecoratorScript), RE::BSFixedString(action.eligibility),
            RE::BSFixedString(kDecoratorScript), RE::BSFixedString(action.execution),
            RE::BSFixedString(""), RE::BSFixedString(kActionCategory), std::int32_t{ kActionPriority }, RE::BSFixedString(""),
            RE::BSFixedString(""), RE::BSFixedString("")));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new ActionResult(stamp, action.name));
        if (!vm->DispatchStaticCall(kApiScript, kRegisterAction, args.get(), callback)) {
            logger::warn("SkyrimNet: could not call SkyrimNetApi.RegisterAction for {}, so it is not offered this load.", action.name);
        }
    }
}

void SkyrimNetBridge::OnDecoratorResult(std::uint32_t stamp, std::optional<std::int32_t> result)
{
    if (stamp != _load.load()) {
        return;
    }
    // SkyrimNetApi.psc does not say what RegisterDecorator returns: any integer answer means the call reached it.
    if (result) {
        _decorator = Decorator::Registered;
        logger::info("SkyrimNet registered the decorator romantasy_bond (result {}).", *result);
    } else {
        _decorator = Decorator::Failed;
        logger::warn("SkyrimNet did not register the decorator romantasy_bond (result {}); the bond line is off for this load.",
            Raw(result));
    }
}

void SkyrimNetBridge::OnActionResult(std::uint32_t stamp, const char* action, std::optional<std::int32_t> result)
{
    if (stamp != _load.load()) {
        return;
    }
    // As with the decorator, any integer answer counts as registered; the raw value is logged either way.
    if (result) {
        logger::info("SkyrimNet registered the action {} (result {}).", action, *result);
    } else {
        logger::warn("SkyrimNet did not register the action {} (result {}).", action, Raw(result));
    }
}

void SkyrimNetBridge::SendEvent(std::string text, RE::FormID follower)
{
    if (!_found.load() || follower == 0 || text.empty() || !Settings::GetSingleton().SkyrimNetBridge()) {
        return;
    }
    const auto send = g_registerEvent.load();
    if (!send) {
        return;
    }
    // No originator: an event from the follower would be shown as their own speech. The follower is the target and
    // the only audience, so bystanders see nothing. An unloaded follower is ignored (and logged) by SkyrimNet.
    const std::uint32_t audience[1]{ follower };
    // The export answers with the event's id, or 0 when it stored nothing (a follower who is not loaded, or a shape
    // it does not take).
    const int id = send(kEventType, text.c_str(), 0, follower, audience, 1);
    logger::debug("SkyrimNet event for {:08X}: {} (id {})", follower, text, id);
    // No fallback to the follower as originator: a missing history line is better than one shown as spoken by them.
    if (id == 0 && !g_nothingStoredLogged.exchange(true)) {
        logger::info("SkyrimNet stored nothing for a bond event. It ignores a follower who is not loaded; if this shows for a follower beside you, bond events are not reaching it.");
    }
}
