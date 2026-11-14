#include "PapyrusBridge.h"

#include "romance/Mood.h"
#include "romance/RomanceManager.h"
#include "settings/Settings.h"

#include <array>
#include <atomic>
#include <cmath>

namespace
{
    constexpr std::string_view kScriptName = "Romantasy";
    constexpr std::int32_t kApiVersion = 9;
    // A script of its own for SkyrimNet's decorator and actions, so the public Romantasy script and its API version stay as they are.
    constexpr std::string_view kSkyrimNetScriptName = "Romantasy_SkyrimNet";

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

    std::int32_t GetMood(RE::StaticFunctionTag*, RE::Actor* followerActor)
    {
        return RomanceManager::GetSingleton().QueryMood(followerActor);
    }

    RE::BSFixedString GetMoodName(RE::StaticFunctionTag*, RE::Actor* followerActor)
    {
        const auto mood = RomanceManager::GetSingleton().QueryMood(followerActor);
        if (mood == 0) {
            return RE::BSFixedString("");
        }
        return RE::BSFixedString(romantasy::mood::Name(static_cast<romantasy::mood::State>(mood)));
    }

    std::int32_t GetMomentCount(RE::StaticFunctionTag*, RE::Actor* followerActor)
    {
        return RomanceManager::GetSingleton().GetMomentCount(followerActor);
    }

    float GetDaysSinceMoment(RE::StaticFunctionTag*, RE::Actor* followerActor)
    {
        return RomanceManager::GetSingleton().GetDaysSinceMoment(followerActor);
    }

    std::int32_t TryShareMoment(RE::StaticFunctionTag*, RE::Actor* followerActor, RE::BSFixedString kind)
    {
        return RomanceManager::GetSingleton().TryShareMoment(followerActor, std::string_view(kind));
    }

    std::int32_t CanShareMoment(RE::StaticFunctionTag*, RE::Actor* followerActor, RE::BSFixedString kind)
    {
        return RomanceManager::GetSingleton().CanShareMoment(followerActor, std::string_view(kind));
    }

    // The author debug functions. Each refuses, before anything else, unless settings.json has
    // "authorMode": true; the refusal is logged once per session per function.
    enum DebugNative : std::size_t
    {
        kDeed,
        kSetPoints,
        kSetDaysTogether,
        kSetMood,
        kAnnounceNow,
        kDescribe,
        kSharedMoment,
        kDebugNativeCount
    };

    std::array<std::atomic_bool, kDebugNativeCount> g_refusalLogged{};

    void LogRefusal(DebugNative native, std::string_view name)
    {
        if (!g_refusalLogged[native].exchange(true)) {
            logger::info("Romantasy {} did nothing: author mode is off. Set \"authorMode\": true in settings.json to use it.", name);
        }
    }

    bool DebugDeed(RE::StaticFunctionTag*, RE::Actor* follower, RE::BSFixedString statistic, std::int32_t count)
    {
        if (!Settings::GetSingleton().AuthorMode()) {
            LogRefusal(kDeed, "DebugDeed");
            return false;
        }
        if (count <= 0) {
            logger::info("Romantasy DebugDeed did nothing: count must be 1 or more (got {})", count);
            return false;
        }
        return RomanceManager::GetSingleton().DebugDeed(follower, std::string_view(statistic), count);
    }

    bool DebugSetPoints(RE::StaticFunctionTag*, RE::Actor* follower, std::int32_t points)
    {
        if (!Settings::GetSingleton().AuthorMode()) {
            LogRefusal(kSetPoints, "DebugSetPoints");
            return false;
        }
        return RomanceManager::GetSingleton().DebugSetPoints(follower, points);
    }

    bool DebugSetDaysTogether(RE::StaticFunctionTag*, RE::Actor* follower, float days)
    {
        if (!Settings::GetSingleton().AuthorMode()) {
            LogRefusal(kSetDaysTogether, "DebugSetDaysTogether");
            return false;
        }
        if (!std::isfinite(days)) {
            logger::info("Romantasy DebugSetDaysTogether did nothing: days is not a number");
            return false;
        }
        return RomanceManager::GetSingleton().DebugSetDaysTogether(follower, days);
    }

    bool DebugSetMood(RE::StaticFunctionTag*, RE::Actor* follower, std::int32_t score)
    {
        if (!Settings::GetSingleton().AuthorMode()) {
            LogRefusal(kSetMood, "DebugSetMood");
            return false;
        }
        return RomanceManager::GetSingleton().DebugSetMood(follower, score);
    }

    bool DebugAnnounceNow(RE::StaticFunctionTag*)
    {
        if (!Settings::GetSingleton().AuthorMode()) {
            LogRefusal(kAnnounceNow, "DebugAnnounceNow");
            return false;
        }
        return RomanceManager::GetSingleton().DebugAnnounceNow();
    }

    bool DebugSharedMoment(
        RE::StaticFunctionTag*, RE::Actor* follower, RE::BSFixedString kind, float seconds, bool others, bool force)
    {
        if (!Settings::GetSingleton().AuthorMode()) {
            LogRefusal(kSharedMoment, "DebugSharedMoment");
            return false;
        }
        return RomanceManager::GetSingleton().DebugSharedMoment(follower, std::string_view(kind), seconds, others, force);
    }

    // SkyrimNet's romantasy_bond decorator calls this through Romantasy_SkyrimNet.BondDecorator, on the Papyrus VM
    // thread, while it builds a prompt. "" for anyone without a switched-on bond, or while the bridge is off.
    RE::BSFixedString GetBondLine(RE::StaticFunctionTag*, RE::Actor* actor)
    {
        try {
            if (!actor) {
                return RE::BSFixedString("");
            }
            return RE::BSFixedString(RomanceManager::GetSingleton().SkyrimNetBondLine(actor));
        } catch (...) {
            return RE::BSFixedString("");
        }
    }

    // SkyrimNet asks Romantasy_SkyrimNet.TouchedEligible or HurtEligible whether an actor may take the action, on the
    // Papyrus VM thread. Changes nothing.
    bool ConversationEligible(RE::StaticFunctionTag*, RE::Actor* actor, bool touched)
    {
        try {
            return actor && RomanceManager::GetSingleton().ConversationEligible(actor, touched);
        } catch (...) {
            return false;
        }
    }

    // SkyrimNet ran Romantasy_SkyrimNet.TouchedExecute or HurtExecute: the bond moves by a few capped points.
    bool ConversationNudge(RE::StaticFunctionTag*, RE::Actor* actor, bool touched)
    {
        try {
            return actor && RomanceManager::GetSingleton().ConversationNudge(actor, touched);
        } catch (...) {
            return false;
        }
    }

    RE::BSFixedString DebugDescribe(RE::StaticFunctionTag*, RE::Actor* follower)
    {
        if (!Settings::GetSingleton().AuthorMode()) {
            LogRefusal(kDescribe, "DebugDescribe");
            return RE::BSFixedString("");
        }
        return RE::BSFixedString(RomanceManager::GetSingleton().DebugDescribe(follower));
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
        vm->RegisterFunction("GetMood", kScriptName, GetMood);
        vm->RegisterFunction("GetMoodName", kScriptName, GetMoodName);
        vm->RegisterFunction("DebugDeed", kScriptName, DebugDeed);
        vm->RegisterFunction("DebugSetPoints", kScriptName, DebugSetPoints);
        vm->RegisterFunction("DebugSetDaysTogether", kScriptName, DebugSetDaysTogether);
        vm->RegisterFunction("DebugSetMood", kScriptName, DebugSetMood);
        vm->RegisterFunction("DebugAnnounceNow", kScriptName, DebugAnnounceNow);
        vm->RegisterFunction("DebugDescribe", kScriptName, DebugDescribe);
        vm->RegisterFunction("GetMomentCount", kScriptName, GetMomentCount);
        vm->RegisterFunction("GetDaysSinceMoment", kScriptName, GetDaysSinceMoment);
        vm->RegisterFunction("TryShareMoment", kScriptName, TryShareMoment);
        vm->RegisterFunction("CanShareMoment", kScriptName, CanShareMoment);
        vm->RegisterFunction("DebugSharedMoment", kScriptName, DebugSharedMoment);
        vm->RegisterFunction("GetBondLine", kSkyrimNetScriptName, GetBondLine);
        vm->RegisterFunction("ConversationEligible", kSkyrimNetScriptName, ConversationEligible);
        vm->RegisterFunction("ConversationNudge", kSkyrimNetScriptName, ConversationNudge);
        logger::info("Romantasy Papyrus bridge registered");
        return true;
    }
}
