#include "conditions/RomanceConditionHook.h"

#include "conditions/RomanceConditionQuery.h"
#include "romance/RomanceManager.h"

#include <atomic>

namespace
{
    RE::SCRIPT_FUNCTION::Condition_t* g_original = nullptr;
    std::atomic_bool g_enabled = false;
    // A synthetic CTDA verifies that the engine passes its BSFixedString pointer
    // through unchanged before we ever interpret a real condition's parameter.
    thread_local const void* g_probeParameter = nullptr;
    thread_local bool g_probeObserved = false;

    bool Evaluate(RE::TESObjectREFR* subject, void* param1, void* param2, double& result)
    {
        if (g_probeParameter) {
            g_probeObserved = param1 == g_probeParameter;
            result = g_probeObserved ? 12345.0 : 0.0;
            return g_probeObserved;
        }
        if (!g_enabled.load() || !param1) {
            return g_original(subject, param1, param2, result);
        }

        const auto& name = *static_cast<const RE::BSFixedString*>(param1);
        const auto query = romantasy::conditions::ParseQuery(name.c_str());
        if (query == romantasy::conditions::Query::None) {
            return g_original(subject, param1, param2, result);
        }

        auto* actor = subject ? subject->As<RE::Actor>() : nullptr;
        result = *romantasy::conditions::Evaluate(query, RomanceManager::GetSingleton().QueryPoints(actor));
        return true;
    }
}

bool RomanceConditionHook::Install()
{
    if (g_enabled.load()) {
        return true;
    }
    auto* command = RE::SCRIPT_FUNCTION::LocateScriptCommand("GetGraphVariableInt");
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!command || !command->conditionFunction || !command->params ||
        command->numParams != 1 || command->params[0].paramType != RE::SCRIPT_PARAM_TYPE::kChar || !player) {
        logger::critical("Romantasy dialogue bridge disabled: unexpected GetGraphVariableInt signature or missing player");
        return false;
    }

    g_original = command->conditionFunction;
    const auto replacement = &Evaluate;
    const auto destination = reinterpret_cast<std::uintptr_t>(std::addressof(command->conditionFunction));
    if (!REL::safe_write(destination, std::addressof(replacement), sizeof(replacement),
            std::addressof(g_original), sizeof(g_original))) {
        logger::critical("Romantasy dialogue bridge disabled: callback replacement verification failed");
        return false;
    }

    RE::BSFixedString name("Romantasy_Registered");
    RE::TESConditionItem condition;
    condition.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetGraphVariableInt;
    condition.data.functionData.params[0] = std::addressof(name);
    condition.data.comparisonValue.f = 12345.0f;
    RE::ConditionCheckParams params(player, player);
    g_probeParameter = std::addressof(name);
    g_probeObserved = false;
    const bool evaluated = condition.IsTrue(params);
    g_probeParameter = nullptr;
    if (!evaluated || !g_probeObserved) {
        const bool restored = REL::safe_write(destination, std::addressof(g_original), sizeof(g_original),
            std::addressof(replacement), sizeof(replacement));
        logger::critical("Romantasy dialogue bridge disabled: CTDA parameter probe failed (callback restored={})", restored);
        return false;
    }

    g_enabled.store(true);
    logger::info("Romantasy dialogue bridge installed; CTDA BSFixedString parameter probe passed");
    return true;
}
