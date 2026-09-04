#include "conditions/PointsConditionHook.h"

#include "romance/RomanceManager.h"

namespace
{
    constexpr std::string_view kRomantasyPlugin = "CS_Romantasy.esp";
    constexpr RE::FormID kRomancePointsLocalID = 0x83D;

    RE::TESFaction* g_romancePointsFaction = nullptr;
    RE::SCRIPT_FUNCTION::Condition_t* g_originalGetFactionRank = nullptr;
    bool g_installed = false;

    bool EvaluateGetFactionRank(
        RE::TESObjectREFR* a_subject,
        void* a_param1,
        void* a_param2,
        double& a_result)
    {
        if (a_param1 != g_romancePointsFaction) {
            return g_originalGetFactionRank(a_subject, a_param1, a_param2, a_result);
        }

        a_result = -1.0;
        if (auto* actor = a_subject ? a_subject->As<RE::Actor>() : nullptr) {
            std::int32_t points = 0;
            if (RomanceManager::GetSingleton().TryGetPoints(actor, points)) {
                a_result = static_cast<double>(points);
            }
        }

        return true;
    }

    RE::TESFaction* LookupRomancePointsFaction()
    {
        if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
            if (auto* faction = dataHandler->LookupForm<RE::TESFaction>(kRomancePointsLocalID, kRomantasyPlugin)) {
                return faction;
            }
        }

        return RE::TESForm::LookupByEditorID<RE::TESFaction>("ROM_RomancePoints");
    }
}

bool PointsConditionHook::Install()
{
    if (g_installed) {
        return true;
    }

    g_romancePointsFaction = LookupRomancePointsFaction();
    if (!g_romancePointsFaction) {
        logger::critical("Romantasy could not find ROM_RomancePoints; raw-points dialogue conditions are disabled");
        return false;
    }

    auto* command = RE::SCRIPT_FUNCTION::LocateScriptCommand("GetFactionRank");
    if (!command || !command->conditionFunction) {
        logger::critical("Romantasy could not locate the GetFactionRank condition callback");
        g_romancePointsFaction = nullptr;
        return false;
    }

    g_originalGetFactionRank = command->conditionFunction;
    const auto replacement = &EvaluateGetFactionRank;
    const auto destination = reinterpret_cast<std::uintptr_t>(std::addressof(command->conditionFunction));
    if (!REL::safe_write(
            destination,
            std::addressof(replacement),
            sizeof(replacement),
            std::addressof(g_originalGetFactionRank),
            sizeof(g_originalGetFactionRank))) {
        logger::critical("Romantasy refused to replace the GetFactionRank condition callback because verification failed");
        g_originalGetFactionRank = nullptr;
        g_romancePointsFaction = nullptr;
        return false;
    }

    g_installed = true;
    logger::info(
        "Romantasy raw-points dialogue condition installed with proxy faction {:08X}",
        g_romancePointsFaction->GetFormID());
    return true;
}
