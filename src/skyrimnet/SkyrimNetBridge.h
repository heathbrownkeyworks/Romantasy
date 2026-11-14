#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

// The optional bridge to SkyrimNet. SkyrimNet is found at run time among the loaded modules, never loaded or linked
// by Romantasy; without it every call here does nothing. This file and SkyrimNetBridge.cpp are the only ones that know
// SkyrimNet is there.
class SkyrimNetBridge
{
public:
    static SkyrimNetBridge& GetSingleton();

    // Game thread, once, at data-loaded after Settings::Load. Logs one line saying what was found.
    void Start();

    // A SkyrimNet the bridge can use is loaded (its API is new enough and the event export resolved).
    [[nodiscard]] bool Found() const { return _found.load(); }
    // SkyrimNet is loaded but its API is older than the bridge needs; the bridge stays off.
    [[nodiscard]] bool TooOld() const { return _tooOld.load(); }
    // What PublicGetVersion returned; 0 when SkyrimNet is absent or the export is missing.
    [[nodiscard]] std::uint32_t ApiVersion() const { return _apiVersion.load(); }
    // Game thread, on every new game and loaded save (the VM is ready). When SkyrimNet was found or is too old, whatever
    // the bridge setting says, dispatches SkyrimNetApi.RegisterDecorator("romantasy_bond", "Romantasy_SkyrimNet", "BondDecorator")
    // as a static Papyrus call, then the two conversation actions (RegisterActions); the results arrive later on the
    // VM thread. Each call starts a new load number, so an answer from an earlier load is ignored.
    void RegisterDecorator();
    // True only after SkyrimNet answered this load's registration with an integer.
    [[nodiscard]] bool DecoratorRegistered() const { return _decorator.load() == Decorator::Registered; }
    // True when this load's registration could not be dispatched or SkyrimNet answered with something not an integer.
    [[nodiscard]] bool DecoratorFailed() const { return _decorator.load() == Decorator::Failed; }
    // The VM thread: SkyrimNet's answer (nullopt when it was not an int) to the dispatch stamped `stamp`.
    void OnDecoratorResult(std::uint32_t stamp, std::optional<std::int32_t> result);
    // The VM thread: SkyrimNet's answer to RegisterAction for `action` (a static name), dispatched under `stamp`.
    void OnActionResult(std::uint32_t stamp, const char* action, std::optional<std::int32_t> result);

    // Game thread. A plain-text event with no originator, for the follower (a reference FormID) as target and only
    // audience. Nothing when SkyrimNet was not found, the bridge is switched off, or the follower is 0.
    void SendEvent(std::string text, RE::FormID follower);

private:
    enum class Decorator : std::uint8_t { NotTried, Pending, Registered, Failed };

    SkyrimNetBridge() = default;

    // Game thread, from RegisterDecorator: SkyrimNetApi.RegisterAction for RomantasyTouched and RomantasyHurt, run by
    // Romantasy_SkyrimNet's Papyrus functions. Registered whatever the settings say, and with a SkyrimNet too old;
    // eligibility answers false while either setting is off or SkyrimNet is not fully found.
    void RegisterActions();

    std::atomic_bool _started = false;
    std::atomic_bool _found = false;
    std::atomic_bool _tooOld = false;
    std::atomic<std::uint32_t> _apiVersion = 0;
    std::atomic<Decorator> _decorator = Decorator::NotTried;
    // The load number every dispatch is stamped with; an answer carrying an older one is ignored.
    std::atomic<std::uint32_t> _load = 0;
};
