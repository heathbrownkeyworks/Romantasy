#include "PapyrusBridge.h"

#include "romance/RomanceManager.h"

namespace
{
    constexpr std::string_view kScriptName = "Romantasy";
    constexpr std::int32_t kApiVersion = 6;

    std::int32_t GetApiVersion(RE::StaticFunctionTag*)
    {
        return kApiVersion;
    }

    bool ModifyPoints(
        RE::StaticFunctionTag*,
        RE::Actor* followerActor,
        std::int32_t pointsDelta,
        RE::BSFixedString reason,
        bool showLevelUp)
    {
        return RomanceManager::GetSingleton().ModifyPoints(
            followerActor,
            pointsDelta,
            std::string_view(reason),
            showLevelUp
        );
    }

    bool ApplyPreference(
        RE::StaticFunctionTag*,
        RE::Actor* followerActor,
        RE::BSFixedString statName,
        std::int32_t delta,
        bool showLevelUp)
    {
        return RomanceManager::GetSingleton().ApplyPreference(
            followerActor,
            std::string_view(statName),
            delta,
            showLevelUp
        );
    }

    bool ClearPreferences(RE::StaticFunctionTag*, RE::Actor* followerActor)
    {
        return RomanceManager::GetSingleton().ClearPreferences(followerActor);
    }

    bool SetPreference(
        RE::StaticFunctionTag*,
        RE::Actor* followerActor,
        RE::BSFixedString statName,
        std::int32_t direction)
    {
        return RomanceManager::GetSingleton().SetPreference(
            followerActor,
            std::string_view(statName),
            direction);
    }

    std::int32_t GetPreference(
        RE::StaticFunctionTag*,
        RE::Actor* followerActor,
        RE::BSFixedString statName)
    {
        return RomanceManager::GetSingleton().GetPreference(
            followerActor,
            std::string_view(statName));
    }

    bool SetPreferencesManual(
        RE::StaticFunctionTag*,
        RE::Actor* followerActor,
        bool manual)
    {
        return RomanceManager::GetSingleton().SetPreferencesManual(followerActor, manual);
    }

    bool IsPreferencesManual(RE::StaticFunctionTag*, RE::Actor* followerActor)
    {
        return RomanceManager::GetSingleton().IsPreferencesManual(followerActor);
    }

    bool RegisterAuthorFollower(
        RE::StaticFunctionTag*,
        RE::Actor* followerActor,
        std::vector<RE::BSFixedString> statNames,
        std::vector<std::int32_t> directions,
        std::int32_t startingLevel,
        RE::TESGlobal* relationshipRankMirror)
    {
        std::vector<std::string> names;
        names.reserve(statNames.size());
        for (const auto& statName : statNames) {
            names.emplace_back(statName.c_str());
        }
        return RomanceManager::GetSingleton().RegisterAuthorFollower(
            followerActor,
            names,
            directions,
            startingLevel,
            relationshipRankMirror);
    }

    bool RegisterAuthorFollowerPointsMirror(
        RE::StaticFunctionTag*,
        RE::Actor* followerActor,
        RE::TESGlobal* relationshipPointsMirror)
    {
        return RomanceManager::GetSingleton().RegisterAuthorFollowerPointsMirror(
            followerActor,
            relationshipPointsMirror);
    }

    std::int32_t GetPoints(RE::StaticFunctionTag*, RE::Actor* followerActor)
    {
        return RomanceManager::GetSingleton().GetPoints(followerActor);
    }

    std::int32_t GetLevel(RE::StaticFunctionTag*, RE::Actor* followerActor)
    {
        return RomanceManager::GetSingleton().GetLevel(followerActor);
    }

    RE::BSFixedString GetLevelName(RE::StaticFunctionTag*, RE::Actor* followerActor)
    {
        return RE::BSFixedString(RomanceManager::GetSingleton().GetLevelName(followerActor));
    }
}

namespace PapyrusBridge
{
    bool Register(RE::BSScript::IVirtualMachine* vm)
    {
        if (!vm) {
            logger::critical("Romantasy Papyrus registration failed; VM unavailable");
            return false;
        }

        vm->RegisterFunction("GetApiVersion", kScriptName, GetApiVersion);
        vm->RegisterFunction("ModifyPoints", kScriptName, ModifyPoints);
        vm->RegisterFunction("ApplyPreference", kScriptName, ApplyPreference);
        vm->RegisterFunction("ClearPreferences", kScriptName, ClearPreferences);
        vm->RegisterFunction("SetPreference", kScriptName, SetPreference);
        vm->RegisterFunction("GetPreference", kScriptName, GetPreference);
        vm->RegisterFunction("SetPreferencesManual", kScriptName, SetPreferencesManual);
        vm->RegisterFunction("IsPreferencesManual", kScriptName, IsPreferencesManual);
        vm->RegisterFunction("RegisterAuthorFollower", kScriptName, RegisterAuthorFollower);
        vm->RegisterFunction("RegisterAuthorFollowerPointsMirror", kScriptName, RegisterAuthorFollowerPointsMirror);
        vm->RegisterFunction("GetPoints", kScriptName, GetPoints);
        vm->RegisterFunction("GetLevel", kScriptName, GetLevel);
        vm->RegisterFunction("GetLevelName", kScriptName, GetLevelName);
        logger::info("Romantasy Papyrus bridge registered");
        return true;
    }
}
