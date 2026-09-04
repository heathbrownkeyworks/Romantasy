#include "RomanceManager.h"

#include <chrono>

#include "settings/Settings.h"
#include "ui/RomantasyUI.h"

namespace
{
    constexpr std::uint32_t RecordType(char first, char second, char third, char fourth)
    {
        return static_cast<std::uint32_t>(first) |
            (static_cast<std::uint32_t>(second) << 8) |
            (static_cast<std::uint32_t>(third) << 16) |
            (static_cast<std::uint32_t>(fourth) << 24);
    }

    constexpr std::string_view kRomantasyPlugin = "CS_Romantasy.esp";
    constexpr std::string_view kSkyrimPlugin = "Skyrim.esm";
    constexpr std::uint32_t kSerializationID = RecordType('R', 'o', 'M', 'a');
    constexpr std::uint32_t kRomancePointsRecord = RecordType('R', 'o', 'P', 't');
    constexpr std::uint32_t kRomancePointsVersion = 2;
    constexpr std::uint32_t kManualPreferencesRecord = RecordType('R', 'o', 'P', 'm');
    constexpr std::uint32_t kManualPreferencesVersion = 1;
    constexpr std::uint32_t kPlayerEnrollmentRecord = RecordType('R', 'o', 'P', 'e');
    constexpr std::uint32_t kPlayerEnrollmentVersion = 1;
    constexpr RE::FormID kRomanceLevelLocalID = 0x800;
    constexpr RE::FormID kCurrentFollowerFactionLocalID = 0x5C84E;

    constexpr std::array<RomanceManager::StatFactionRule, 58> kStatRules{ {
        { "ROM_LocationsDiscovered", "Locations Discovered", 2, 0x801 },
        { "ROM_DungeonsCleared", "Dungeons Cleared", 2, 0x802 },
        { "ROM_DaysPassed", "Days Passed", 2, 0x803 },
        { "ROM_StandingStonesFound", "Standing Stones Found", 2, 0x804 },
        { "ROM_ChestsLooted", "Chests Looted", 1, 0x805 },
        { "ROM_SkillIncreases", "Skill Increases", 2, 0x806 },
        { "ROM_SkillBooksRead", "Skill Books Read", 1, 0x807 },
        { "ROM_Barters", "Barters", 1, 0x808 },
        { "ROM_Persuasions", "Persuasions", 1, 0x809 },
        { "ROM_Bribes", "Bribes", 1, 0x80A },
        { "ROM_Intimidations", "Intimidations", 1, 0x80B },
        { "ROM_DiseasesContracted", "Diseases Contracted", 1, 0x80C },
        { "ROM_DaysVampire", "Days as a Vampire", 2, 0x80D },
        { "ROM_DaysWerewolf", "Days as a Werewolf", 2, 0x80E },
        { "ROM_NecksBitten", "Necks Bitten", 2, 0x80F },
        { "ROM_VampirismCures", "Vampirism Cures", 2, 0x810 },
        { "ROM_WerewolfTransformations", "Werewolf Transformations", 5, 0x811 },
        { "ROM_Mauls", "Mauls", 2, 0x812 },
        { "ROM_QuestsCompleted", "Quests Completed", 3, 0x813 },
        { "ROM_MiscObjectivesCompleted", "Misc Objectives Completed", 3, 0x814 },
        { "ROM_MainQuestsCompleted", "Main Quests Completed", 5, 0x815 },
        { "ROM_SideQuestsCompleted", "Side Quests Completed", 5, 0x816 },
        { "ROM_CompanionsCompleted", "The Companions Quests Completed", 10, 0x817 },
        { "ROM_CollegeCompleted", "College of Winterhold Quests Completed", 10, 0x818 },
        { "ROM_ThievesCompleted", "Thieves' Guild Quests Completed", 10, 0x819 },
        { "ROM_DarkBrotherhoodCompleted", "The Dark Brotherhood Quests Completed", 10, 0x81A },
        { "ROM_CivilWarCompleted", "Civil War Quests Completed", 10, 0x81B },
        { "ROM_DaedricCompleted", "Daedric Quests Completed", 10, 0x81C },
        { "ROM_DawnguardCompleted", "Dawnguard Quests Completed", 10, 0x81D },
        { "ROM_DragonbornCompleted", "Dragonborn Quests Completed", 10, 0x81E },
        { "ROM_QuestlinesCompleted", "Questlines Completed", 20, 0x81F },
        { "ROM_PeopleKilled", "People Killed", 1, 0x820 },
        { "ROM_AnimalsKilled", "Animals Killed", 1, 0x821 },
        { "ROM_CreaturesKilled", "Creatures Killed", 1, 0x822 },
        { "ROM_UndeadKilled", "Undead Killed", 1, 0x823 },
        { "ROM_DaedraKilled", "Daedra Killed", 1, 0x824 },
        { "ROM_AutomatonsKilled", "Automatons Killed", 1, 0x825 },
        { "ROM_CriticalStrikes", "Critical Strikes", 1, 0x826 },
        { "ROM_SneakAttacks", "Sneak Attacks", 1, 0x827 },
        { "ROM_Backstabs", "Backstabs", 1, 0x828 },
        { "ROM_WeaponsDisarmed", "Weapons Disarmed", 1, 0x829 },
        { "ROM_BunniesSlaughtered", "Bunnies Slaughtered", 1, 0x82A },
        { "ROM_SpellsLearned", "Spells Learned", 2, 0x82B },
        { "ROM_DragonSoulsCollected", "Dragon Souls Collected", 2, 0x82C },
        { "ROM_ShoutsLearned", "Shouts Learned", 2, 0x82D },
        { "ROM_SoulsTrapped", "Souls Trapped", 1, 0x82E },
        { "ROM_MagicItemsMade", "Magic Items Made", 1, 0x82F },
        { "ROM_WeaponsMade", "Weapons Made", 1, 0x830 },
        { "ROM_ArmorMade", "Armor Made", 1, 0x831 },
        { "ROM_PotionsMixed", "Potions Mixed", 1, 0x832 },
        { "ROM_PoisonsMixed", "Poisons Mixed", 1, 0x833 },
        { "ROM_LocksPicked", "Locks Picked", 1, 0x834 },
        { "ROM_PocketsPicked", "Pockets Picked", 1, 0x835 },
        { "ROM_ItemsStolen", "Items Stolen", 1, 0x836 },
        { "ROM_Assaults", "Assaults", 2, 0x837 },
        { "ROM_Murders", "Murders", 5, 0x838 },
        { "ROM_HorsesStolen", "Horses Stolen", 2, 0x839 },
        { "ROM_Trespasses", "Trespasses", 2, 0x83A },
    } };

    template <typename T>
    bool ReadRecord(SKSE::SerializationInterface* serialization, T& value)
    {
        return serialization->ReadRecordData(value) == sizeof(T);
    }

    struct SerializedRomancePointsV1
    {
        RE::FormID baseFormID = 0;
        std::int32_t points = 0;
    };

    struct SerializedRomancePointsV2
    {
        RE::FormID referenceFormID = 0;
        RE::FormID baseFormID = 0;
        std::int32_t points = 0;
    };

    void PushRecentEvent(RomanceFollower& follower, std::string label, std::int32_t points)
    {
        constexpr std::size_t kMaxRecentEvents = 5;
        float gameDay = 0.0f;
        if (const auto* calendar = RE::Calendar::GetSingleton()) {
            gameDay = calendar->GetDaysPassed();
        }
        follower.recent.insert(follower.recent.begin(), { std::move(label), points, gameDay });
        if (follower.recent.size() > kMaxRecentEvents) {
            follower.recent.resize(kMaxRecentEvents);
        }
    }

    // Builds a "Race · Class" descriptor for the UI eyebrow (e.g. "Nord · Necromancer").
    // Returns empty when the base NPC / race / class is unavailable; the UI then falls back to "Companion".
    std::string BuildRoleLabel(const RomanceFollower& follower)
    {
        auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(follower.baseFormID);
        if (!npc) {
            return {};
        }

        std::string race;
        if (auto* raceForm = npc->GetRace()) {
            if (const char* fullName = raceForm->GetFullName()) {
                race = fullName;
            }
        }

        std::string className;
        if (auto* classForm = npc->npcClass) {
            if (const char* fullName = classForm->GetFullName()) {
                className = fullName;
            }
        }

        if (!race.empty() && !className.empty()) {
            return race + " \xC2\xB7 " + className;  // middle dot separator
        }
        return race.empty() ? className : race;
    }

    // Broadcasts a Romantasy event to any Papyrus script that called
    // RegisterForModEvent. strArg may be empty; sender is the follower's base
    // NPC form so listeners can identify which follower the event is for.
    void SendBarkEvent(const char* eventName, const char* strArg, float numArg, RE::TESForm* sender)
    {
        // Dispatch game-to-Papyrus callbacks on the main thread because callers may
        // arrive from the UI, tracked-stat, or combat event paths.
        SKSE::GetTaskInterface()->AddTask(
            [name = RE::BSFixedString(eventName), str = RE::BSFixedString(strArg), numArg, sender]() {
                auto* source = SKSE::GetModCallbackEventSource();
                if (!source) {
                    return;
                }
                SKSE::ModCallbackEvent modEvent{ name, str, numArg, sender };
                source->SendEvent(&modEvent);
            });
    }
}

RomanceManager& RomanceManager::GetSingleton()
{
    static RomanceManager singleton;
    return singleton;
}

void RomanceManager::RegisterSerializationCallbacks(const SKSE::SerializationInterface* serialization)
{
    if (!serialization) {
        logger::critical("Romantasy failed to acquire SKSE serialization interface");
        return;
    }

    serialization->SetUniqueID(kSerializationID);
    serialization->SetSaveCallback(SaveCallback);
    serialization->SetLoadCallback(LoadCallback);
    serialization->SetRevertCallback(RevertCallback);
    logger::info("Romantasy serialization callbacks registered");
}

void RomanceManager::Initialize()
{
    std::scoped_lock lock(_lock);
    _romanceLevelFaction = LookupFaction("ROM_RomanceLevel", kRomanceLevelLocalID, kRomantasyPlugin);
    CaptureAuthorDefinedBases();
    _currentFollowerFaction = LookupFaction("CurrentFollowerFaction", kCurrentFollowerFactionLocalID, kSkyrimPlugin);
    if (!_currentFollowerFaction) {
        logger::warn("Romantasy could not find CurrentFollowerFaction; active follower checks will use player teammate state only");
    }

    _statFactions.clear();
    _statRulesByName.clear();
    _statRulesByNormalizedName.clear();
    for (const auto& rule : kStatRules) {
        if (auto* faction = LookupFaction(rule.editorID, rule.fallbackLocalID, kRomantasyPlugin)) {
            _statFactions.emplace(rule.editorID, faction);
            const auto fullLabel = LabelForRule(rule);
            _statRulesByName.emplace(fullLabel, std::addressof(rule));
            _statRulesByName.emplace(std::string(rule.label), std::addressof(rule));
            _statRulesByName.emplace(std::string(rule.editorID), std::addressof(rule));
            _statRulesByNormalizedName.emplace(NormalizeStatName(fullLabel), std::addressof(rule));
            _statRulesByNormalizedName.emplace(NormalizeStatName(rule.label), std::addressof(rule));
            _statRulesByNormalizedName.emplace(NormalizeStatName(rule.editorID), std::addressof(rule));
        } else {
            logger::warn("Romantasy stat faction not found: {}", rule.editorID);
        }
    }

    _followers = DiscoverFollowers();
    ApplySavedPoints();
    MirrorRelationshipLevels();
    RegisterStatsSink();
    logger::info("Romantasy discovered {} romanceable NPC base records", _followers.size());
}

void RomanceManager::DebugAddPoints(std::int32_t points)
{
    if (points == 0) {
        return;
    }

    std::scoped_lock lock(_lock);
    EnsureFollowersDiscovered();
    RefreshLoadedFollowers();
    logger::info("Romantasy debug adding {} points to {} follower(s)", points, _followers.size());
    bool changedAnyFollower = false;
    for (auto& follower : _followers) {
        changedAnyFollower = AddPoints(follower, points, "Debug adjustment", true, false) || changedAnyFollower;
    }

    if (changedAnyFollower) {
        SendRuntimeState("Debug adjustment applied.");
    }
}

void RomanceManager::DebugApplyStat(std::string_view statName, std::int32_t delta)
{
    if (delta <= 0) {
        return;
    }

    std::scoped_lock lock(_lock);
    EnsureFollowersDiscovered();
    const auto* rule = FindStatRule(statName);
    if (!rule) {
        logger::warn("Romantasy debug stat not recognized: {}", statName);
        return;
    }

    logger::info("Romantasy debug applying stat {} x{}", statName, delta);
    ApplyStatDelta(*rule, delta, true, false);
}

bool RomanceManager::ModifyPoints(RE::Actor* followerActor, std::int32_t pointsDelta, std::string_view reason, bool showLevelUp)
{
    if (pointsDelta == 0) {
        return true;
    }

    std::scoped_lock lock(_lock);
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::warn("Romantasy ModifyPoints failed; follower actor was not romanceable or was unavailable");
        return false;
    }

    const auto eventLabel = reason.empty() ? std::string("Authored event") : std::string(reason);
    if (AddPoints(*follower, pointsDelta, eventLabel, showLevelUp)) {
        SendRuntimeState("Romance records updated.");
        return true;
    }

    return false;
}

bool RomanceManager::ApplyPreference(RE::Actor* followerActor, std::string_view statName, std::int32_t delta, bool showLevelUp)
{
    if (delta <= 0) {
        return false;
    }

    std::scoped_lock lock(_lock);
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::warn("Romantasy ApplyPreference failed; follower actor was not romanceable or was unavailable");
        return false;
    }

    const auto* rule = FindStatRule(statName);
    if (!rule) {
        logger::warn("Romantasy ApplyPreference failed; stat was not recognized: {}", statName);
        return false;
    }

    RefreshFollowerPreferences(*follower, followerActor, rule);
    const auto direction = follower->statDirections.find(rule->editorID);
    if (direction == follower->statDirections.end() || direction->second == 0) {
        logger::info("Romantasy ApplyPreference ignored {}; {} has no configured preference", statName, follower->name);
        return false;
    }

    const auto pointsDelta = delta * rule->points * direction->second;
    if (pointsDelta == 0) {
        return false;
    }

    if (AddPoints(*follower, pointsDelta, LabelForRule(*rule), showLevelUp)) {
        SendRuntimeState("Romance records updated.");
        return true;
    }

    return false;
}

bool RomanceManager::ClearPreferences(RE::Actor* followerActor)
{
    std::scoped_lock lock(_lock);
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::warn("Romantasy ClearPreferences failed; follower actor was not romanceable or was unavailable");
        return false;
    }

    if (follower->origin == RomanceProfileOrigin::kAuthorDefined) {
        logger::warn("Romantasy ClearPreferences rejected for author-defined follower {}", follower->name);
        return false;
    }

    for (const auto& [editorID, faction] : _statFactions) {
        if (faction && followerActor->IsInFaction(faction)) {
            followerActor->RemoveFromFaction(faction);
        }
    }

    RefreshFollowerPreferences(*follower, followerActor);
    logger::info("Romantasy cleared all preferences for {} ({:08X})", follower->name, followerActor->GetFormID());
    SendRuntimeState("Follower preferences cleared.");
    return true;
}

bool RomanceManager::SetPreference(RE::Actor* followerActor, std::string_view statName, std::int32_t direction)
{
    std::scoped_lock lock(_lock);
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::warn("Romantasy SetPreference failed; follower actor was not romanceable or was unavailable");
        return false;
    }

    if (follower->origin == RomanceProfileOrigin::kAuthorDefined) {
        logger::warn("Romantasy SetPreference rejected for author-defined follower {}", follower->name);
        return false;
    }

    const auto* rule = FindStatRule(statName);
    if (!rule) {
        logger::warn("Romantasy SetPreference failed; stat was not recognized: {}", statName);
        return false;
    }

    const auto foundFaction = _statFactions.find(rule->editorID);
    if (foundFaction == _statFactions.end() || !foundFaction->second) {
        logger::warn("Romantasy SetPreference failed; preference faction was unavailable: {}", rule->editorID);
        return false;
    }

    auto* faction = foundFaction->second;
    if (direction == 0) {
        if (followerActor->IsInFaction(faction)) {
            followerActor->RemoveFromFaction(faction);
        }
    } else {
        followerActor->AddToFaction(faction, direction < 0 ? 0 : 1);
    }

    RefreshFollowerPreferences(*follower, followerActor, rule);
    logger::info(
        "Romantasy set {} preference for {} ({:08X}) to {}",
        rule->editorID,
        follower->name,
        followerActor->GetFormID(),
        direction < 0 ? "dislike" : direction > 0 ? "like" : "neutral");
    SendRuntimeState("Follower preferences updated.");
    return true;
}

std::int32_t RomanceManager::GetPreference(RE::Actor* followerActor, std::string_view statName)
{
    std::scoped_lock lock(_lock);
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::warn("Romantasy GetPreference failed; follower actor was not romanceable or was unavailable");
        return 0;
    }

    const auto* rule = FindStatRule(statName);
    if (!rule) {
        logger::warn("Romantasy GetPreference failed; stat was not recognized: {}", statName);
        return 0;
    }

    RefreshFollowerPreferences(*follower, followerActor, rule);
    if (const auto direction = follower->statDirections.find(rule->editorID);
        direction != follower->statDirections.end()) {
        return direction->second < 0 ? -1 : direction->second > 0 ? 1 : 0;
    }
    return 0;
}

bool RomanceManager::SetPreferencesManual(RE::Actor* followerActor, bool manual)
{
    std::scoped_lock lock(_lock);
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::warn("Romantasy SetPreferencesManual failed; follower actor was not romanceable or was unavailable");
        return false;
    }

    if (follower->origin == RomanceProfileOrigin::kAuthorDefined) {
        logger::warn("Romantasy SetPreferencesManual rejected for author-defined follower {}", follower->name);
        return false;
    }
    if (follower->origin == RomanceProfileOrigin::kPlayerCreated && !manual) {
        logger::warn("Romantasy cannot release player-created preferences for {}", follower->name);
        return false;
    }

    const auto identityFormID = SavedPointsKey(*follower);
    if (identityFormID == 0) {
        logger::warn("Romantasy SetPreferencesManual failed; follower identity was unavailable");
        return false;
    }

    if (manual) {
        _manualPreferenceActors.insert(identityFormID);
    } else {
        _manualPreferenceActors.erase(identityFormID);
        _manualPreferenceActors.erase(follower->baseFormID);
    }

    logger::info(
        "Romantasy marked preferences for {} ({:08X}) as {}",
        follower->name,
        identityFormID,
        manual ? "player-managed" : "author-managed");
    SendRuntimeState(manual ? "Preferences marked as player-managed." : "Preferences released to mod authors.");
    return true;
}

bool RomanceManager::IsPreferencesManual(RE::Actor* followerActor)
{
    std::scoped_lock lock(_lock);
    if (const auto* follower = FindFollower(followerActor)) {
        return IsPreferencesManual(*follower);
    }
    return false;
}

bool RomanceManager::RegisterAuthorFollower(
    RE::Actor* followerActor,
    const std::vector<std::string>& statNames,
    const std::vector<std::int32_t>& directions,
    std::int32_t startingLevel,
    RE::TESGlobal* relationshipRankMirror)
{
    std::scoped_lock lock(_lock);
    if (!followerActor || !_romanceLevelFaction || !IsValidAuthorFollowerStartingLevel(startingLevel)) {
        logger::warn("Romantasy author registration rejected; actor, romance faction, or starting level was invalid");
        return false;
    }

    auto* baseNpc = followerActor->GetActorBase();
    const auto identityFormID = ActorIdentity(followerActor);
    if (!baseNpc || identityFormID == 0) {
        logger::warn("Romantasy author registration rejected; actor identity or base record was unavailable");
        return false;
    }
    if (statNames.size() != directions.size() || statNames.size() > kStatRules.size()) {
        logger::warn(
            "Romantasy author registration rejected; received {} statistic name(s) and {} direction(s)",
            statNames.size(),
            directions.size());
        return false;
    }
    if (_statFactions.size() != kStatRules.size()) {
        logger::warn("Romantasy author registration rejected; one or more preference factions were unavailable");
        return false;
    }

    std::vector<std::pair<RE::TESFaction*, std::int32_t>> resolvedPreferences;
    resolvedPreferences.reserve(statNames.size());
    std::unordered_set<RE::TESFaction*> seenFactions;
    for (std::size_t index = 0; index < statNames.size(); ++index) {
        const auto direction = directions[index];
        if (!IsValidAuthorPreferenceDirection(direction)) {
            logger::warn(
                "Romantasy author registration rejected invalid direction {} for {}",
                direction,
                statNames[index]);
            return false;
        }
        const auto* rule = FindStatRule(statNames[index]);
        if (!rule) {
            logger::warn("Romantasy author registration rejected unknown statistic {}", statNames[index]);
            return false;
        }
        const auto factionIt = _statFactions.find(rule->editorID);
        if (factionIt == _statFactions.end() || !factionIt->second) {
            logger::warn("Romantasy author registration could not resolve faction {}", rule->editorID);
            return false;
        }
        if (!seenFactions.insert(factionIt->second).second) {
            logger::warn("Romantasy author registration specified {} more than once", rule->editorID);
            return false;
        }
        resolvedPreferences.emplace_back(factionIt->second, direction);
    }

    const auto baseFormID = baseNpc->GetFormID();
    const auto startingPoints = MinimumPointsForFactionRank(static_cast<std::int8_t>(startingLevel - 1));
    auto preservedPoints = startingPoints;
    if (const auto* cachedFollower = FindCachedFollower(followerActor)) {
        preservedPoints = cachedFollower->points;
    } else if (const auto saved = _savedPoints.find(identityFormID); saved != _savedPoints.end()) {
        preservedPoints = saved->second;
    } else if (const auto baseSaved = _savedPoints.find(baseFormID); baseSaved != _savedPoints.end()) {
        preservedPoints = baseSaved->second;
    } else if (followerActor->IsInFaction(_romanceLevelFaction)) {
        const auto existingRank = static_cast<std::int8_t>(
            followerActor->GetFactionRank(_romanceLevelFaction, false));
        preservedPoints = MinimumPointsForFactionRank(existingRank);
    }

    _authorIntegrationBaseForms.insert(baseFormID);
    _playerEnrolledActors.erase(identityFormID);
    _playerEnrolledActors.erase(baseFormID);
    _manualPreferenceActors.erase(identityFormID);
    _manualPreferenceActors.erase(baseFormID);

    for (const auto& [editorID, faction] : _statFactions) {
        if (followerActor->IsInFaction(faction)) {
            followerActor->RemoveFromFaction(faction);
        }
    }
    for (const auto& [faction, direction] : resolvedPreferences) {
        followerActor->AddToFaction(faction, direction < 0 ? 0 : 1);
    }

    _savedPoints[identityFormID] = preservedPoints;
    followerActor->AddToFaction(_romanceLevelFaction, FactionRankForPoints(preservedPoints));
    _relationshipRankMirrors.erase(identityFormID);
    _relationshipRankMirrors.erase(baseFormID);
    if (relationshipRankMirror) {
        _relationshipRankMirrors[identityFormID] = relationshipRankMirror;
    }

    (void)RefreshRuntimeFollower(followerActor);
    auto* follower = FindCachedFollower(followerActor);
    if (!follower) {
        logger::error("Romantasy author registration failed to refresh the follower cache");
        return false;
    }
    follower->origin = RomanceProfileOrigin::kAuthorDefined;
    follower->basePoints = startingPoints;
    follower->points = preservedPoints;
    RefreshFollowerPreferences(*follower, followerActor);
    MirrorRelationshipLevel(*follower);

    logger::info(
        "Romantasy registered author follower {} (reference {:08X}, base {:08X}) with {} preference(s) and {} preserved point(s)",
        follower->name,
        identityFormID,
        baseFormID,
        resolvedPreferences.size(),
        preservedPoints);
    SendRuntimeState("Author follower integration registered.");
    return true;
}

bool RomanceManager::RegisterAuthorFollowerPointsMirror(
    RE::Actor* followerActor,
    RE::TESGlobal* relationshipPointsMirror)
{
    std::scoped_lock lock(_lock);
    if (!followerActor || !relationshipPointsMirror) {
        logger::warn("Romantasy points-mirror registration rejected; actor or global was unavailable");
        return false;
    }

    auto* baseNpc = followerActor->GetActorBase();
    const auto identityFormID = ActorIdentity(followerActor);
    if (!baseNpc || identityFormID == 0 || !_authorIntegrationBaseForms.contains(baseNpc->GetFormID())) {
        logger::warn("Romantasy points-mirror registration rejected; actor was not registered by an author integration");
        return false;
    }

    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::warn("Romantasy points-mirror registration rejected; follower state was unavailable");
        return false;
    }

    _relationshipPointsMirrors.erase(identityFormID);
    _relationshipPointsMirrors.erase(baseNpc->GetFormID());
    _relationshipPointsMirrors[identityFormID] = relationshipPointsMirror;
    relationshipPointsMirror->value = static_cast<float>(follower->points);
    logger::info(
        "Romantasy registered relationship-points mirror for {} ({:08X}) at {} point(s)",
        follower->name,
        identityFormID,
        follower->points);
    return true;
}

bool RomanceManager::EnrollPlayerFollower(
    RE::Actor* followerActor,
    const std::unordered_map<std::string, std::int32_t>& preferences)
{
    std::scoped_lock lock(_lock);
    if (!IsEligibleEnrollmentCandidate(followerActor)) {
        logger::warn("Romantasy player enrollment rejected; actor was ineligible or already managed");
        return false;
    }

    std::vector<std::pair<RE::TESFaction*, std::int32_t>> resolvedPreferences;
    if (!ValidatePreferenceProfile(preferences, resolvedPreferences)) {
        return false;
    }

    const auto identityFormID = ActorIdentity(followerActor);
    auto* baseNpc = followerActor->GetActorBase();
    if (identityFormID == 0 || !baseNpc || !_romanceLevelFaction) {
        logger::warn("Romantasy player enrollment rejected; actor identity or romance faction was unavailable");
        return false;
    }

    _playerEnrolledActors.insert(identityFormID);
    _manualPreferenceActors.insert(identityFormID);
    _savedPoints[identityFormID] = 0;
    followerActor->AddToFaction(_romanceLevelFaction, 0);
    for (const auto& [faction, direction] : resolvedPreferences) {
        if (direction != 0) {
            followerActor->AddToFaction(faction, direction < 0 ? 0 : 1);
        }
    }

    if (!RefreshRuntimeFollower(followerActor)) {
        if (auto* follower = FindCachedFollower(followerActor)) {
            follower->origin = RomanceProfileOrigin::kPlayerCreated;
            follower->basePoints = 0;
            follower->points = 0;
            RefreshFollowerPreferences(*follower, followerActor);
        }
    }

    logger::info(
        "Romantasy player-enrolled {} (reference {:08X}, base {:08X}) with {} configured preference(s)",
        SafeString(followerActor->GetDisplayFullName()),
        identityFormID,
        baseNpc->GetFormID(),
        std::ranges::count_if(resolvedPreferences, [](const auto& entry) { return entry.second != 0; }));
    SendRuntimeState("A new bond was entered in the ledger.");
    return true;
}

bool RomanceManager::ReplacePlayerPreferences(
    RE::Actor* followerActor,
    const std::unordered_map<std::string, std::int32_t>& preferences)
{
    std::scoped_lock lock(_lock);
    auto* follower = FindFollower(followerActor);
    if (!follower || follower->origin != RomanceProfileOrigin::kPlayerCreated) {
        logger::warn("Romantasy preference edit rejected; profile was not player-created");
        return false;
    }

    std::vector<std::pair<RE::TESFaction*, std::int32_t>> resolvedPreferences;
    if (!ValidatePreferenceProfile(preferences, resolvedPreferences)) {
        return false;
    }

    for (const auto& [editorID, faction] : _statFactions) {
        if (faction && followerActor->IsInFaction(faction)) {
            followerActor->RemoveFromFaction(faction);
        }
    }
    for (const auto& [faction, direction] : resolvedPreferences) {
        if (direction != 0) {
            followerActor->AddToFaction(faction, direction < 0 ? 0 : 1);
        }
    }

    _manualPreferenceActors.insert(SavedPointsKey(*follower));
    RefreshFollowerPreferences(*follower, followerActor);
    logger::info("Romantasy replaced the player-created personality for {}", follower->name);
    SendRuntimeState("Companion personality sealed.");
    return true;
}

bool RomanceManager::ResetPlayerFollowerPoints(RE::Actor* followerActor)
{
    std::scoped_lock lock(_lock);
    auto* follower = FindFollower(followerActor);
    if (!follower || follower->origin != RomanceProfileOrigin::kPlayerCreated) {
        logger::warn("Romantasy point reset rejected; profile was not player-created");
        return false;
    }

    follower->points = 0;
    follower->basePoints = 0;
    follower->recent.clear();
    _savedPoints[SavedPointsKey(*follower)] = 0;
    RefreshFollowerPreferences(*follower, followerActor);
    MirrorRelationshipLevel(*follower);
    logger::info("Romantasy reset the player-created bond with {}", follower->name);
    SendRuntimeState("Player-created bond reset.");
    return true;
}

bool RomanceManager::RemovePlayerFollower(RE::Actor* followerActor)
{
    std::scoped_lock lock(_lock);
    auto* follower = FindFollower(followerActor);
    if (!follower || follower->origin != RomanceProfileOrigin::kPlayerCreated || IsAuthorDefinedActor(followerActor)) {
        logger::warn("Romantasy removal rejected; profile was not exclusively player-created");
        return false;
    }

    const auto identityFormID = SavedPointsKey(*follower);
    const auto baseFormID = follower->baseFormID;
    const auto followerName = follower->name;
    for (const auto& [editorID, faction] : _statFactions) {
        if (faction && followerActor->IsInFaction(faction)) {
            followerActor->RemoveFromFaction(faction);
        }
    }
    if (_romanceLevelFaction && followerActor->IsInFaction(_romanceLevelFaction)) {
        followerActor->RemoveFromFaction(_romanceLevelFaction);
    }

    _savedPoints.erase(identityFormID);
    _manualPreferenceActors.erase(identityFormID);
    _playerEnrolledActors.erase(identityFormID);
    std::erase_if(_followers, [&](const auto& entry) {
        return entry.referenceFormID == identityFormID ||
            (entry.referenceFormID == 0 && entry.baseFormID == baseFormID);
    });

    logger::info("Romantasy removed player-created bond {} ({:08X})", followerName, identityFormID);
    SendRuntimeState("Player-created bond removed.");
    return true;
}

std::int32_t RomanceManager::GetPoints(RE::Actor* followerActor)
{
    std::int32_t points = 0;
    (void)TryGetPoints(followerActor, points);
    return points;
}

bool RomanceManager::TryGetPoints(RE::Actor* followerActor, std::int32_t& points)
{
    std::scoped_lock lock(_lock);
    if (const auto* follower = FindFollower(followerActor)) {
        points = follower->points;
        return true;
    }

    points = 0;
    return false;
}

std::int32_t RomanceManager::GetLevel(RE::Actor* followerActor)
{
    std::scoped_lock lock(_lock);
    if (const auto* follower = FindFollower(followerActor)) {
        return LevelNumberForPoints(follower->points);
    }

    return 0;
}

std::string RomanceManager::GetLevelName(RE::Actor* followerActor)
{
    std::scoped_lock lock(_lock);
    if (const auto* follower = FindFollower(followerActor)) {
        return std::string(LevelNameForPoints(follower->points));
    }

    return {};
}

nlohmann::json RomanceManager::BuildStateJson(std::string_view message)
{
    std::scoped_lock lock(_lock);
    RefreshLoadedFollowers();
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    float currentDay = 0.0f;
    if (const auto* calendar = RE::Calendar::GetSingleton()) {
        currentDay = calendar->GetDaysPassed();
    }

    for (auto& follower : _followers) {
        if (auto* actor = ResolveFollowerActor(follower)) {
            RefreshFollowerIdentity(follower, actor);
        }
    }
    std::ranges::sort(_followers, [](const auto& lhs, const auto& rhs) {
        return lhs.name < rhs.name;
    });

    nlohmann::json romances = nlohmann::json::array();
    for (const auto& follower : _followers) {
        nlohmann::json recent = nlohmann::json::array();
        for (const auto& event : follower.recent) {
            nlohmann::json entry = {
                { "label", event.label },
                { "points", event.points },
            };
            if (event.gameDay > 0.0f && currentDay >= event.gameDay) {
                const auto daysAgo = static_cast<std::int32_t>(currentDay - event.gameDay);
                if (daysAgo <= 0) {
                    entry["when"] = "Today";
                } else if (daysAgo == 1) {
                    entry["when"] = "Yesterday";
                } else {
                    entry["when"] = std::format("{} days ago", daysAgo);
                }
            }
            recent.push_back(std::move(entry));
        }

        romances.push_back({
            { "name", follower.name },
            { "editorID", follower.editorID },
            { "baseFormID", std::format("{:08X}", follower.baseFormID) },
            { "referenceFormID", std::format("{:08X}", follower.referenceFormID) },
            { "preferencesManual", IsPreferencesManual(follower) },
            { "profileOrigin", ProfileOriginName(follower.origin) },
            { "personalityEditable", follower.origin == RomanceProfileOrigin::kPlayerCreated },
            { "removable", follower.origin == RomanceProfileOrigin::kPlayerCreated },
            { "sourcePlugin", SourcePluginForActor(ResolveFollowerActor(follower)) },
            { "points", follower.points },
            { "levelName", std::string(LevelNameForPoints(follower.points)) },
            { "nextGoal", NextLevelNameForPoints(follower.points) },
            { "isFollowing", IsFollowerFollowing(follower) },
            { "role", BuildRoleLabel(follower) },
            { "likes", follower.likes },
            { "dislikes", follower.dislikes },
            { "recent", recent },
        });
    }

    return {
        { "plugin", Plugin::NAME },
        { "message", message },
        { "timestamp", seconds },
        { "openWithFavorites", Settings::GetSingleton().OpenWithFavorites() },
        { "showGainModals", Settings::GetSingleton().ShowGainModals() },
        { "showLossModals", Settings::GetSingleton().ShowLossModals() },
        { "showAwayFollowers", Settings::GetSingleton().ShowAwayFollowers() },
        { "romances", romances },
        { "enrollmentCandidates", BuildEnrollmentCandidatesJson() },
        { "preferenceOptions", BuildPreferenceOptionsJson() },
    };
}

nlohmann::json RomanceManager::BuildLevelChangeJson(
    const RomanceFollower& follower,
    std::int32_t oldLevel,
    std::int32_t newLevel,
    std::int32_t pointsDelta) const
{
    const auto previousLevelName = std::string(LevelNameForPoints(MinimumPointsForFactionRank(
        static_cast<std::int8_t>((std::max)(0, oldLevel - 1))
    )));
    const auto currentLevelName = std::string(LevelNameForPoints(follower.points));

    return {
        { "followerName", follower.name },
        { "changeDirection", newLevel < oldLevel ? "loss" : "gain" },
        { "previousLevelName", previousLevelName },
        { "previousLevelIndex", oldLevel },
        { "levelName", currentLevelName },
        { "levelIndex", newLevel },
        { "nextLevelName", NextLevelNameForPoints(follower.points) },
        { "points", follower.points },
        { "pointsDelta", pointsDelta },
    };
}

std::string_view RomanceManager::LevelNameForPoints(std::int32_t points)
{
    if (points >= 2500) {
        return "Spouse";
    }
    if (points >= 2000) {
        return "Lover";
    }
    if (points >= 1500) {
        return "Confidant";
    }
    if (points >= 1000) {
        return "Friend";
    }
    if (points >= 500) {
        return "Acquaintance";
    }
    return "Stranger";
}

std::int32_t RomanceManager::LevelNumberForPoints(std::int32_t points)
{
    if (points >= 2500) {
        return 6;
    }
    if (points >= 2000) {
        return 5;
    }
    if (points >= 1500) {
        return 4;
    }
    if (points >= 1000) {
        return 3;
    }
    if (points >= 500) {
        return 2;
    }
    return 1;
}

std::int32_t RomanceManager::MinimumPointsForFactionRank(std::int8_t rank)
{
    switch (std::clamp<std::int8_t>(rank, 0, 5)) {
    case 5:
        return 2500;
    case 4:
        return 2000;
    case 3:
        return 1500;
    case 2:
        return 1000;
    case 1:
        return 500;
    default:
        return 0;
    }
}

std::int8_t RomanceManager::FactionRankForPoints(std::int32_t points)
{
    return static_cast<std::int8_t>((std::max)(0, LevelNumberForPoints(points) - 1));
}

std::string RomanceManager::NextLevelNameForPoints(std::int32_t points)
{
    if (points >= 2500) {
        return "Vow sealed";
    }
    if (points >= 2000) {
        return "Spouse";
    }
    if (points >= 1500) {
        return "Lover";
    }
    if (points >= 1000) {
        return "Confidant";
    }
    if (points >= 500) {
        return "Friend";
    }
    return "Acquaintance";
}

std::string RomanceManager::NormalizeStatName(std::string_view statName)
{
    std::string normalized;
    normalized.reserve(statName.size());
    for (const auto character : statName) {
        const auto value = static_cast<unsigned char>(character);
        if (std::isalnum(value)) {
            normalized.push_back(static_cast<char>(std::tolower(value)));
        }
    }

    return normalized;
}

std::string RomanceManager::SafeString(const char* value)
{
    return value && value[0] != '\0' ? std::string(value) : std::string{};
}

std::string_view RomanceManager::ProfileOriginName(RomanceProfileOrigin origin)
{
    switch (origin) {
    case RomanceProfileOrigin::kAuthorDefined:
        return "author";
    case RomanceProfileOrigin::kPlayerCreated:
        return "player";
    default:
        return "external";
    }
}

std::string_view RomanceManager::PreferenceCategory(const StatFactionRule& rule)
{
    if (rule.fallbackLocalID <= 0x807) {
        return "Exploration & Growth";
    }
    if (rule.fallbackLocalID <= 0x80B) {
        return "Influence";
    }
    if (rule.fallbackLocalID <= 0x812) {
        return "Blood & Beast";
    }
    if (rule.fallbackLocalID <= 0x81F) {
        return "Quests & Allegiances";
    }
    if (rule.fallbackLocalID <= 0x82A) {
        return "Battle";
    }
    if (rule.fallbackLocalID <= 0x833) {
        return "Magic & Craft";
    }
    return "Crime & Transgression";
}

nlohmann::json RomanceManager::BuildEnrollmentCandidatesJson() const
{
    std::vector<nlohmann::json> candidates;
    std::unordered_set<RE::FormID> seen;
    if (auto* processLists = RE::ProcessLists::GetSingleton()) {
        processLists->ForEachHighActor([&](RE::Actor* actor) {
            if (!IsEligibleEnrollmentCandidate(actor)) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            const auto identityFormID = ActorIdentity(actor);
            if (identityFormID == 0 || !seen.insert(identityFormID).second) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            RomanceFollower candidate;
            candidate.baseFormID = actor->GetActorBase() ? actor->GetActorBase()->GetFormID() : 0;
            candidate.referenceFormID = identityFormID;
            candidate.name = DisplayNameForActor(actor, candidate);
            candidates.push_back({
                { "name", candidate.name },
                { "referenceFormID", std::format("{:08X}", candidate.referenceFormID) },
                { "baseFormID", std::format("{:08X}", candidate.baseFormID) },
                { "role", BuildRoleLabel(candidate) },
                { "sourcePlugin", SourcePluginForActor(actor) },
            });
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }

    std::ranges::sort(candidates, [](const auto& lhs, const auto& rhs) {
        return lhs.value("name", std::string{}) < rhs.value("name", std::string{});
    });
    nlohmann::json result = nlohmann::json::array();
    for (auto& candidate : candidates) {
        result.push_back(std::move(candidate));
    }
    return result;
}

nlohmann::json RomanceManager::BuildPreferenceOptionsJson() const
{
    nlohmann::json options = nlohmann::json::array();
    for (const auto& rule : kStatRules) {
        options.push_back({
            { "editorID", rule.editorID },
            { "label", LabelForRule(rule) },
            { "points", rule.points },
            { "category", PreferenceCategory(rule) },
        });
    }
    return options;
}

RE::Actor* RomanceManager::ResolveFollowerActor(const RomanceFollower& follower) const
{
    if (follower.referenceFormID != 0) {
        return RE::TESForm::LookupByID<RE::Actor>(follower.referenceFormID);
    }

    if (auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(follower.baseFormID)) {
        return npc->GetUniqueActor();
    }

    return nullptr;
}

RE::FormID RomanceManager::SavedPointsKey(const RomanceFollower& follower) const
{
    return follower.referenceFormID != 0 ? follower.referenceFormID : follower.baseFormID;
}

RE::FormID RomanceManager::ActorIdentity(RE::Actor* followerActor) const
{
    if (!followerActor) {
        return 0;
    }
    const auto referenceFormID = followerActor->GetFormID();
    if (referenceFormID != 0) {
        return referenceFormID;
    }
    if (auto* baseNpc = followerActor->GetActorBase()) {
        return baseNpc->GetFormID();
    }
    return 0;
}

std::string RomanceManager::SourcePluginForActor(RE::Actor* followerActor) const
{
    if (followerActor) {
        if (auto* baseNpc = followerActor->GetActorBase()) {
            if (auto* file = baseNpc->GetFile(0)) {
                return std::string(file->GetFilename());
            }
        }
    }
    return {};
}

void RomanceManager::CaptureAuthorDefinedBases()
{
    _authorDefinedBaseForms.clear();
    if (!_romanceLevelFaction) {
        return;
    }

    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        logger::warn("Romantasy could not capture author-defined follower bases; data handler unavailable");
        return;
    }

    for (auto* npc : dataHandler->GetFormArray<RE::TESNPC>()) {
        if (npc && !npc->IsDeleted() && npc->IsInFaction(_romanceLevelFaction)) {
            _authorDefinedBaseForms.insert(npc->GetFormID());
        }
    }

    logger::info(
        "Romantasy captured {} author-defined follower base record(s)",
        _authorDefinedBaseForms.size());
}

bool RomanceManager::IsAuthorDefinedActor(RE::Actor* followerActor) const
{
    if (!followerActor) {
        return false;
    }
    auto* baseNpc = followerActor->GetActorBase();
    return baseNpc && (_authorDefinedBaseForms.contains(baseNpc->GetFormID()) ||
        _authorIntegrationBaseForms.contains(baseNpc->GetFormID()));
}

RomanceProfileOrigin RomanceManager::ProfileOriginForActor(RE::Actor* followerActor) const
{
    const auto identityFormID = ActorIdentity(followerActor);
    auto* baseNpc = followerActor ? followerActor->GetActorBase() : nullptr;
    const auto playerEnrolled = _playerEnrolledActors.contains(identityFormID) ||
        (baseNpc && _playerEnrolledActors.contains(baseNpc->GetFormID()));
    return ClassifyRomanceProfileOrigin(IsAuthorDefinedActor(followerActor), playerEnrolled);
}

bool RomanceManager::HasAnyPreferenceFaction(RE::Actor* followerActor) const
{
    if (!followerActor) {
        return false;
    }
    return std::ranges::any_of(_statFactions, [&](const auto& entry) {
        return entry.second && followerActor->IsInFaction(entry.second);
    });
}

bool RomanceManager::IsEligibleEnrollmentCandidate(RE::Actor* followerActor) const
{
    if (!followerActor || followerActor == RE::PlayerCharacter::GetSingleton() ||
        followerActor->IsChild() || followerActor->IsDead() || !followerActor->GetActorBase() ||
        !_romanceLevelFaction || !IsActorFollowing(followerActor)) {
        return false;
    }
    if (followerActor->IsInFaction(_romanceLevelFaction) || HasAnyPreferenceFaction(followerActor)) {
        return false;
    }

    const auto identityFormID = ActorIdentity(followerActor);
    if (identityFormID == 0 || _playerEnrolledActors.contains(identityFormID)) {
        return false;
    }
    const auto alreadyCached = std::ranges::any_of(_followers, [&](const auto& follower) {
        return follower.referenceFormID == identityFormID ||
            (follower.referenceFormID == 0 && follower.baseFormID == followerActor->GetActorBase()->GetFormID());
    });
    return !alreadyCached && !DisplayNameForActor(followerActor, {}).empty();
}

bool RomanceManager::ValidatePreferenceProfile(
    const std::unordered_map<std::string, std::int32_t>& preferences,
    std::vector<std::pair<RE::TESFaction*, std::int32_t>>& resolved) const
{
    resolved.clear();
    resolved.reserve(preferences.size());
    std::unordered_set<RE::TESFaction*> seenFactions;
    for (const auto& [statName, direction] : preferences) {
        if (direction < -1 || direction > 1) {
            logger::warn("Romantasy preference profile rejected invalid direction {} for {}", direction, statName);
            return false;
        }
        const auto* rule = FindStatRule(statName);
        if (!rule) {
            logger::warn("Romantasy preference profile rejected unknown statistic {}", statName);
            return false;
        }
        const auto factionIt = _statFactions.find(rule->editorID);
        if (factionIt == _statFactions.end() || !factionIt->second) {
            logger::warn("Romantasy preference profile could not resolve faction {}", rule->editorID);
            return false;
        }
        if (!seenFactions.insert(factionIt->second).second) {
            logger::warn("Romantasy preference profile specified {} more than once", rule->editorID);
            return false;
        }
        resolved.emplace_back(factionIt->second, direction);
    }
    return true;
}

bool RomanceManager::IsPreferencesManual(const RomanceFollower& follower) const
{
    if (follower.origin == RomanceProfileOrigin::kAuthorDefined) {
        return false;
    }
    const auto identityFormID = SavedPointsKey(follower);
    return _manualPreferenceActors.contains(identityFormID) ||
        (follower.referenceFormID != 0 && _manualPreferenceActors.contains(follower.baseFormID));
}

std::string RomanceManager::DisplayNameForActor(RE::Actor* followerActor, const RomanceFollower& follower) const
{
    if (followerActor) {
        const auto displayName = SafeString(followerActor->GetDisplayFullName());
        if (!displayName.empty()) {
            return displayName;
        }
    }

    if (auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(follower.baseFormID)) {
        const auto baseName = SafeString(npc->GetName());
        if (!baseName.empty()) {
            return baseName;
        }
    }

    if (!follower.name.empty()) {
        return follower.name;
    }
    return follower.editorID.empty() ? std::format("{:08X}", follower.baseFormID) : follower.editorID;
}

void RomanceManager::RefreshFollowerIdentity(RomanceFollower& follower, RE::Actor* followerActor)
{
    if (!followerActor) {
        return;
    }

    follower.referenceFormID = followerActor->GetFormID();
    follower.name = DisplayNameForActor(followerActor, follower);
}

RomanceFollower* RomanceManager::FindFollower(RE::Actor* followerActor)
{
    if (!followerActor) {
        return nullptr;
    }

    EnsureFollowersDiscovered();

    if (auto* follower = FindCachedFollower(followerActor)) {
        RefreshFollowerIdentity(*follower, followerActor);
        return follower;
    }

    (void)RefreshRuntimeFollower(followerActor);
    return FindCachedFollower(followerActor);
}

RomanceFollower* RomanceManager::FindCachedFollower(RE::Actor* followerActor)
{
    if (!followerActor) {
        return nullptr;
    }

    const auto referenceFormID = followerActor->GetFormID();
    const auto exact = std::ranges::find_if(_followers, [&](const auto& candidate) {
        return candidate.referenceFormID != 0 && candidate.referenceFormID == referenceFormID;
    });
    if (exact != _followers.end()) {
        return std::addressof(*exact);
    }

    auto* baseNpc = followerActor->GetActorBase();
    if (!baseNpc || !baseNpc->IsUnique()) {
        return nullptr;
    }

    if (auto* uniqueActor = baseNpc->GetUniqueActor(); uniqueActor && uniqueActor != followerActor) {
        return nullptr;
    }

    const auto follower = std::ranges::find_if(_followers, [&](const auto& candidate) {
        return candidate.baseFormID == baseNpc->GetFormID() && candidate.referenceFormID == 0;
    });

    if (follower == _followers.end()) {
        return nullptr;
    }

    auto* matched = std::addressof(*follower);
    RefreshFollowerIdentity(*matched, followerActor);
    return matched;
}

void RomanceManager::RefreshFollowerPreferences(
    RomanceFollower& follower,
    RE::Actor* followerActor,
    const StatFactionRule* rule)
{
    if (!followerActor) {
        return;
    }

    const auto refreshRule = [&](const StatFactionRule& currentRule) {
        const auto foundFaction = _statFactions.find(currentRule.editorID);
        if (foundFaction == _statFactions.end()) {
            return;
        }

        const auto label = LabelForRule(currentRule);
        follower.statDirections.erase(currentRule.editorID);
        std::erase(follower.likes, label);
        std::erase(follower.dislikes, label);

        auto* faction = foundFaction->second;
        if (!followerActor->IsInFaction(faction)) {
            return;
        }

        const auto direction = followerActor->GetFactionRank(faction, false) <= 0 ? -1 : 1;
        follower.statDirections[currentRule.editorID] = direction;
        if (direction < 0) {
            follower.dislikes.push_back(label);
        } else {
            follower.likes.push_back(label);
        }
    };

    if (rule) {
        refreshRule(*rule);
    } else {
        follower.statDirections.clear();
        follower.likes.clear();
        follower.dislikes.clear();
        for (const auto& currentRule : kStatRules) {
            refreshRule(currentRule);
        }
    }

    if (!follower.likes.empty() || !follower.dislikes.empty()) {
        std::erase_if(follower.recent, [](const auto& event) {
            return event.points == 0 && event.label == "Awaiting QueryStat preferences";
        });
    } else if (!rule && follower.recent.empty()) {
        follower.recent.push_back({ "Awaiting QueryStat preferences", 0 });
    }
}

bool RomanceManager::RefreshRuntimeFollower(RE::Actor* followerActor, const StatFactionRule* rule)
{
    if (!followerActor || !_romanceLevelFaction || !followerActor->IsInFaction(_romanceLevelFaction)) {
        return false;
    }

    if (auto* follower = FindCachedFollower(followerActor)) {
        RefreshFollowerIdentity(*follower, followerActor);
        RefreshFollowerPreferences(*follower, followerActor, rule);
        return false;
    }

    auto* baseNpc = followerActor->GetActorBase();
    if (!baseNpc) {
        return false;
    }

    RomanceFollower follower;
    follower.baseFormID = baseNpc->GetFormID();
    follower.referenceFormID = followerActor->GetFormID();
    follower.editorID = SafeString(baseNpc->GetFormEditorID());
    follower.name = DisplayNameForActor(followerActor, follower);
    follower.origin = ProfileOriginForActor(followerActor);

    const auto romanceRank = static_cast<std::int8_t>(followerActor->GetFactionRank(_romanceLevelFaction, false));
    follower.basePoints = MinimumPointsForFactionRank(romanceRank);
    follower.points = follower.basePoints;
    if (const auto saved = _savedPoints.find(follower.referenceFormID); saved != _savedPoints.end()) {
        follower.points = saved->second;
    } else if (const auto legacySaved = _savedPoints.find(follower.baseFormID); legacySaved != _savedPoints.end()) {
        follower.points = legacySaved->second;
        logger::info(
            "Romantasy migrated base-keyed points for dynamic follower {} ({:08X})",
            follower.name,
            follower.referenceFormID);
    }

    RefreshFollowerPreferences(follower, followerActor);
    logger::info(
        "Romantasy runtime-discovered {} (reference {:08X}, base {:08X}, origin={}): points={}, likes={}, dislikes={}",
        follower.name,
        follower.referenceFormID,
        follower.baseFormID,
        ProfileOriginName(follower.origin),
        follower.points,
        follower.likes.size(),
        follower.dislikes.size());

    _followers.push_back(std::move(follower));
    std::ranges::sort(_followers, [](const auto& lhs, const auto& rhs) {
        return lhs.name < rhs.name;
    });

    if (auto* addedFollower = FindCachedFollower(followerActor)) {
        MirrorRelationshipLevel(*addedFollower);
    }
    return true;
}

void RomanceManager::RefreshLoadedFollowers(const StatFactionRule* rule)
{
    auto* processLists = RE::ProcessLists::GetSingleton();
    if (!processLists) {
        return;
    }

    processLists->ForEachHighActor([&](RE::Actor* actor) {
        (void)RefreshRuntimeFollower(actor, rule);
        return RE::BSContainer::ForEachResult::kContinue;
    });

    std::ranges::sort(_followers, [](const auto& lhs, const auto& rhs) {
        return lhs.name < rhs.name;
    });
}

bool RomanceManager::IsActorFollowing(RE::Actor* followerActor) const
{
    if (!followerActor) {
        return false;
    }

    if (_currentFollowerFaction) {
        const auto rank = followerActor->GetFactionRank(_currentFollowerFaction, false);
        if (rank >= 0) {
            return true;
        }
    }

    return followerActor->IsPlayerTeammate();
}

bool RomanceManager::IsFollowerFollowing(const RomanceFollower& follower) const
{
    auto* actor = ResolveFollowerActor(follower);
    if (!actor) {
        logger::info(
            "Romantasy skipped {}; actor reference {:08X} is not currently available for active follower check",
            follower.name,
            follower.referenceFormID);
        return false;
    }

    return IsActorFollowing(actor);
}

const RomanceManager::StatFactionRule* RomanceManager::FindStatRule(std::string_view statName) const
{
    if (const auto exact = _statRulesByName.find(std::string(statName)); exact != _statRulesByName.end()) {
        return exact->second;
    }

    if (const auto normalized = _statRulesByNormalizedName.find(NormalizeStatName(statName));
        normalized != _statRulesByNormalizedName.end()) {
        return normalized->second;
    }

    return nullptr;
}

bool RomanceManager::IsPlayerInCombat() const
{
    const auto* player = RE::PlayerCharacter::GetSingleton();
    return player && player->IsInCombat();
}

std::string RomanceManager::LabelForRule(const StatFactionRule& rule) const
{
    const auto foundFaction = _statFactions.find(rule.editorID);
    if (foundFaction != _statFactions.end()) {
        const auto fullName = SafeString(foundFaction->second->GetFullName());
        if (!fullName.empty()) {
            return fullName;
        }
    }

    return std::string(rule.label);
}

bool RomanceManager::AddPoints(RomanceFollower& follower, std::int32_t pointsDelta, std::string_view reason, bool showLevelUp, bool requireFollowing)
{
    if (pointsDelta == 0) {
        return false;
    }

    if (requireFollowing && !IsFollowerFollowing(follower)) {
        logger::info(
            "Romantasy skipped {} point adjustment for {}; follower is not actively following",
            reason,
            follower.name
        );
        return false;
    }

    const auto oldLevel = LevelNumberForPoints(follower.points);
    follower.points = (std::max)(0, follower.points + pointsDelta);
    _savedPoints[SavedPointsKey(follower)] = follower.points;
    PushRecentEvent(follower, std::string(reason), pointsDelta);
    MirrorRelationshipLevel(follower);

    logger::info(
        "Romantasy {}: {} point(s) for {}; total={}, rank={}",
        reason,
        pointsDelta,
        follower.name,
        follower.points,
        FactionRankForPoints(follower.points)
    );

    const auto newLevel = LevelNumberForPoints(follower.points);
    if (showLevelUp && newLevel != oldLevel) {
        QueueOrShowLevelChange(BuildLevelChangeJson(follower, oldLevel, newLevel, pointsDelta));
        if (newLevel > oldLevel) {
            if (auto* senderForm = RE::TESForm::LookupByID(follower.baseFormID)) {
                SendBarkEvent("Romantasy_OnLevelChanged", "", static_cast<float>(newLevel), senderForm);
            }
        }
    }

    return true;
}

RE::TESFaction* RomanceManager::LookupFaction(
    std::string_view editorID,
    RE::FormID fallbackLocalID,
    std::string_view pluginName) const
{
    if (auto* faction = RE::TESForm::LookupByEditorID<RE::TESFaction>(editorID)) {
        return faction;
    }

    if (fallbackLocalID == 0) {
        return nullptr;
    }

    if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
        return dataHandler->LookupForm<RE::TESFaction>(fallbackLocalID, pluginName);
    }

    return nullptr;
}

void RomanceManager::ApplySavedPoints()
{
    for (auto& follower : _followers) {
        follower.points = follower.basePoints;
        if (const auto points = _savedPoints.find(SavedPointsKey(follower)); points != _savedPoints.end()) {
            follower.points = points->second;
        } else if (follower.referenceFormID != 0) {
            if (const auto legacyPoints = _savedPoints.find(follower.baseFormID); legacyPoints != _savedPoints.end()) {
                follower.points = legacyPoints->second;
            }
        }
    }
}

void RomanceManager::ApplyStatDelta(const StatFactionRule& rule, std::int32_t delta, bool showLevelUp, bool requireFollowing)
{
    if (delta <= 0) {
        return;
    }

    EnsureFollowersDiscovered();
    RefreshLoadedFollowers(std::addressof(rule));
    const auto label = LabelForRule(rule);
    bool changedAnyFollower = false;

    for (auto& follower : _followers) {
        const auto direction = follower.statDirections.find(rule.editorID);
        if (direction == follower.statDirections.end() || direction->second == 0) {
            continue;
        }

        const auto pointsDelta = delta * rule.points * direction->second;
        if (pointsDelta != 0) {
            const auto beforeLevel = LevelNumberForPoints(follower.points);
            const bool changed = AddPoints(follower, pointsDelta, label, showLevelUp, requireFollowing);
            if (changed) {
                changedAnyFollower = true;
                // Deterministic dedup: AddPoints already fired OnLevelChanged if
                // this tick crossed a tier. Only fire the preference event when it
                // did NOT, so a single tick never produces two barks.
                if (LevelNumberForPoints(follower.points) == beforeLevel) {
                    if (auto* senderForm = RE::TESForm::LookupByID(follower.baseFormID)) {
                        SendBarkEvent("Romantasy_OnPreference", label.c_str(), static_cast<float>(pointsDelta), senderForm);
                    }
                }
            }
        }
    }

    if (changedAnyFollower) {
        SendRuntimeState("Romance records updated.");
    }
}

void RomanceManager::EnsureFollowersDiscovered()
{
    if (!_followers.empty()) {
        return;
    }

    _followers = DiscoverFollowers();
    ApplySavedPoints();
    MirrorRelationshipLevels();
    logger::info("Romantasy refreshed follower cache: {} romanceable NPC base records", _followers.size());
}

void RomanceManager::FlushPendingLevelChanges()
{
    if (_pendingLevelChanges.empty() || IsPlayerInCombat()) {
        return;
    }

    logger::info("Romantasy showing {} pending relationship level notification(s) after combat", _pendingLevelChanges.size());
    while (!_pendingLevelChanges.empty()) {
        auto payload = std::move(_pendingLevelChanges.front());
        _pendingLevelChanges.pop_front();
        SKSE::GetTaskInterface()->AddTask([payload = std::move(payload)]() {
            RomantasyUI::GetSingleton().ShowLevelUpModal(payload);
        });
    }
}

void RomanceManager::Load(SKSE::SerializationInterface* serialization)
{
    std::scoped_lock lock(_lock);
    _savedPoints.clear();
    _authorIntegrationBaseForms.clear();
    _relationshipRankMirrors.clear();
    _relationshipPointsMirrors.clear();
    _manualPreferenceActors.clear();
    _playerEnrolledActors.clear();

    std::uint32_t type = 0;
    std::uint32_t version = 0;
    std::uint32_t length = 0;
    while (serialization->GetNextRecordInfo(type, version, length)) {
        if (type == kPlayerEnrollmentRecord) {
            if (version != kPlayerEnrollmentVersion) {
                logger::warn("Romantasy skipping player-enrollment record version {}", version);
                continue;
            }

            std::uint32_t count = 0;
            if (!ReadRecord(serialization, count)) {
                logger::error("Romantasy failed to read player-enrollment identity count");
                return;
            }
            for (std::uint32_t index = 0; index < count; ++index) {
                RE::FormID serializedFormID = 0;
                if (!ReadRecord(serialization, serializedFormID)) {
                    logger::error("Romantasy failed to read player-enrollment identity {}", index);
                    return;
                }
                RE::FormID resolvedFormID = 0;
                if (serialization->ResolveFormID(serializedFormID, resolvedFormID)) {
                    _playerEnrolledActors.insert(resolvedFormID);
                } else {
                    logger::warn("Romantasy could not resolve player-enrollment identity {:08X}", serializedFormID);
                }
            }
            continue;
        }

        if (type == kManualPreferencesRecord) {
            if (version != kManualPreferencesVersion) {
                logger::warn("Romantasy skipping manual-preference record version {}", version);
                continue;
            }

            std::uint32_t count = 0;
            if (!ReadRecord(serialization, count)) {
                logger::error("Romantasy failed to read manual-preference identity count");
                return;
            }

            for (std::uint32_t index = 0; index < count; ++index) {
                RE::FormID serializedFormID = 0;
                if (!ReadRecord(serialization, serializedFormID)) {
                    logger::error("Romantasy failed to read manual-preference identity {}", index);
                    return;
                }

                RE::FormID resolvedFormID = 0;
                if (serialization->ResolveFormID(serializedFormID, resolvedFormID)) {
                    _manualPreferenceActors.insert(resolvedFormID);
                } else {
                    logger::warn("Romantasy could not resolve manual-preference identity {:08X}", serializedFormID);
                }
            }
            continue;
        }

        if (type != kRomancePointsRecord) {
            logger::warn("Romantasy skipping unknown serialization record type {:08X}", type);
            continue;
        }

        if (version != 1 && version != kRomancePointsVersion) {
            logger::warn("Romantasy skipping romance point record version {}", version);
            continue;
        }

        std::uint32_t count = 0;
        if (!ReadRecord(serialization, count)) {
            logger::error("Romantasy failed to read serialized romance point count");
            return;
        }

        for (std::uint32_t index = 0; index < count; ++index) {
            if (version == 1) {
                SerializedRomancePointsV1 serialized;
                if (!ReadRecord(serialization, serialized)) {
                    logger::error("Romantasy failed to read version 1 romance point entry {}", index);
                    return;
                }

                RE::FormID resolvedBaseFormID = 0;
                if (serialization->ResolveFormID(serialized.baseFormID, resolvedBaseFormID)) {
                    _savedPoints[resolvedBaseFormID] = serialized.points;
                } else {
                    logger::warn("Romantasy could not resolve version 1 follower base FormID {:08X}", serialized.baseFormID);
                }
                continue;
            }

            SerializedRomancePointsV2 serialized;
            if (!ReadRecord(serialization, serialized)) {
                logger::error("Romantasy failed to read version 2 romance point entry {}", index);
                return;
            }

            RE::FormID resolvedReferenceFormID = 0;
            if (serialized.referenceFormID != 0 &&
                serialization->ResolveFormID(serialized.referenceFormID, resolvedReferenceFormID)) {
                _savedPoints[resolvedReferenceFormID] = serialized.points;
                continue;
            }

            RE::FormID resolvedBaseFormID = 0;
            if (serialization->ResolveFormID(serialized.baseFormID, resolvedBaseFormID)) {
                _savedPoints[resolvedBaseFormID] = serialized.points;
                if (serialized.referenceFormID != 0) {
                    logger::warn(
                        "Romantasy could not resolve saved follower reference {:08X}; retained points under base {:08X}",
                        serialized.referenceFormID,
                        resolvedBaseFormID);
                }
            } else {
                logger::warn(
                    "Romantasy could not resolve saved follower reference {:08X} or base {:08X}",
                    serialized.referenceFormID,
                    serialized.baseFormID);
            }
        }
    }

    _followers = DiscoverFollowers();
    for (const auto& follower : _followers) {
        if (follower.origin == RomanceProfileOrigin::kAuthorDefined) {
            _manualPreferenceActors.erase(SavedPointsKey(follower));
            _manualPreferenceActors.erase(follower.baseFormID);
        }
    }
    std::vector<RE::FormID> invalidPlayerEnrollments;
    for (const auto identityFormID : _playerEnrolledActors) {
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(identityFormID);
        if (!actor) {
            continue;
        }
        if (IsAuthorDefinedActor(actor)) {
            invalidPlayerEnrollments.push_back(identityFormID);
            logger::warn(
                "Romantasy ignored player-enrollment marker {:08X}; the follower is author-defined",
                identityFormID);
            continue;
        }
        actor->AddToFaction(_romanceLevelFaction, 0);
        _manualPreferenceActors.insert(identityFormID);
        _savedPoints.try_emplace(identityFormID, 0);
        (void)RefreshRuntimeFollower(actor);
    }
    for (const auto identityFormID : invalidPlayerEnrollments) {
        _playerEnrolledActors.erase(identityFormID);
        _manualPreferenceActors.erase(identityFormID);
    }

    std::vector<RE::FormID> savedIdentities;
    savedIdentities.reserve(_savedPoints.size());
    for (const auto& [identityFormID, points] : _savedPoints) {
        savedIdentities.push_back(identityFormID);
    }
    for (const auto identityFormID : savedIdentities) {
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(identityFormID)) {
            (void)RefreshRuntimeFollower(actor);
        }
    }
    ApplySavedPoints();
    MirrorRelationshipLevels();
    logger::info(
        "Romantasy loaded {} serialized romance point entries, {} player-managed preference flag(s), and {} player enrollment(s)",
        _savedPoints.size(),
        _manualPreferenceActors.size(),
        _playerEnrolledActors.size());
}

void RomanceManager::MirrorRelationshipLevel(RomanceFollower& follower)
{
    if (!_romanceLevelFaction) {
        return;
    }

    auto* actor = ResolveFollowerActor(follower);
    if (!actor) {
        logger::info(
            "Romantasy cannot mirror level for {}; actor reference {:08X} is not currently available",
            follower.name,
            follower.referenceFormID);
        return;
    }

    const auto rank = FactionRankForPoints(follower.points);
    actor->AddToFaction(_romanceLevelFaction, rank);
    const auto identityFormID = SavedPointsKey(follower);
    auto mirror = _relationshipRankMirrors.find(identityFormID);
    if (mirror == _relationshipRankMirrors.end() && identityFormID != follower.baseFormID) {
        mirror = _relationshipRankMirrors.find(follower.baseFormID);
    }
    if (mirror != _relationshipRankMirrors.end() && mirror->second) {
        mirror->second->value = static_cast<float>(rank);
    }
    auto pointsMirror = _relationshipPointsMirrors.find(identityFormID);
    if (pointsMirror == _relationshipPointsMirrors.end() && identityFormID != follower.baseFormID) {
        pointsMirror = _relationshipPointsMirrors.find(follower.baseFormID);
    }
    if (pointsMirror != _relationshipPointsMirrors.end() && pointsMirror->second) {
        pointsMirror->second->value = static_cast<float>(follower.points);
    }
    logger::info("Romantasy mirrored {} to ROM_RomanceLevel rank {}", follower.name, rank);
}

void RomanceManager::MirrorRelationshipLevels()
{
    for (auto& follower : _followers) {
        MirrorRelationshipLevel(follower);
    }
}

void RomanceManager::QueueOrShowLevelChange(nlohmann::json payload)
{
    if (IsPlayerInCombat()) {
        logger::info(
            "Romantasy queued relationship level {} notification for {}; player is in combat",
            payload.value("changeDirection", "change"),
            payload.value("followerName", "unknown follower")
        );
        _pendingLevelChanges.push_back(std::move(payload));
        return;
    }

    SKSE::GetTaskInterface()->AddTask([payload = std::move(payload)]() {
        RomantasyUI::GetSingleton().ShowLevelUpModal(payload);
    });
}

void RomanceManager::RegisterStatsSink()
{
    if (auto* events = RE::ScriptEventSourceHolder::GetSingleton()) {
        if (!_statsSinkRegistered) {
            events->AddEventSink<RE::TESTrackedStatsEvent>(this);
            _statsSinkRegistered = true;
            logger::info("Romantasy tracked stats sink registered");
        }

        if (!_combatSinkRegistered) {
            events->AddEventSink<RE::TESCombatEvent>(this);
            _combatSinkRegistered = true;
            logger::info("Romantasy combat sink registered");
        }
    } else {
        logger::critical("Romantasy failed to register event sinks");
    }
}

void RomanceManager::Revert()
{
    std::scoped_lock lock(_lock);
    _lastStatValues.clear();
    _savedPoints.clear();
    _authorIntegrationBaseForms.clear();
    _relationshipRankMirrors.clear();
    _relationshipPointsMirrors.clear();
    _manualPreferenceActors.clear();
    _playerEnrolledActors.clear();
    _pendingLevelChanges.clear();
    _followers = DiscoverFollowers();
    MirrorRelationshipLevels();
    logger::info("Romantasy serialization state reverted; rebuilt {} romanceable follower definition(s)", _followers.size());
}

void RomanceManager::Save(SKSE::SerializationInterface* serialization) const
{
    std::scoped_lock lock(_lock);
    if (!serialization->OpenRecord(kRomancePointsRecord, kRomancePointsVersion)) {
        logger::error("Romantasy failed to open romance point serialization record");
        return;
    }

    std::unordered_map<RE::FormID, SerializedRomancePointsV2> serializedByIdentity;
    for (const auto& [identityFormID, points] : _savedPoints) {
        SerializedRomancePointsV2 serialized;
        serialized.points = points;
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(identityFormID)) {
            serialized.referenceFormID = identityFormID;
            if (auto* baseNpc = actor->GetActorBase()) {
                serialized.baseFormID = baseNpc->GetFormID();
            }
        } else {
            serialized.baseFormID = identityFormID;
        }
        serializedByIdentity[identityFormID] = serialized;
    }

    for (const auto& follower : _followers) {
        if (follower.referenceFormID != 0) {
            if (auto* baseNpc = RE::TESForm::LookupByID<RE::TESNPC>(follower.baseFormID); baseNpc && baseNpc->IsUnique()) {
                serializedByIdentity.erase(follower.baseFormID);
            }
        }

        serializedByIdentity[SavedPointsKey(follower)] = SerializedRomancePointsV2{
            .referenceFormID = follower.referenceFormID,
            .baseFormID = follower.baseFormID,
            .points = follower.points,
        };
    }

    const auto count = static_cast<std::uint32_t>(serializedByIdentity.size());
    if (!serialization->WriteRecordData(count)) {
        logger::error("Romantasy failed to write romance point count");
        return;
    }

    for (const auto& [identityFormID, serialized] : serializedByIdentity) {
        if (!serialization->WriteRecordData(serialized)) {
            logger::error("Romantasy failed to write romance points for identity {:08X}", identityFormID);
            return;
        }
    }

    logger::info("Romantasy saved {} romance point entries", count);

    if (!serialization->OpenRecord(kManualPreferencesRecord, kManualPreferencesVersion)) {
        logger::error("Romantasy failed to open manual-preference serialization record");
        return;
    }

    const auto manualCount = static_cast<std::uint32_t>(_manualPreferenceActors.size());
    if (!serialization->WriteRecordData(manualCount)) {
        logger::error("Romantasy failed to write manual-preference identity count");
        return;
    }

    for (const auto identityFormID : _manualPreferenceActors) {
        if (!serialization->WriteRecordData(identityFormID)) {
            logger::error("Romantasy failed to write manual-preference identity {:08X}", identityFormID);
            return;
        }
    }

    logger::info("Romantasy saved {} player-managed preference flag(s)", manualCount);

    if (!serialization->OpenRecord(kPlayerEnrollmentRecord, kPlayerEnrollmentVersion)) {
        logger::error("Romantasy failed to open player-enrollment serialization record");
        return;
    }

    const auto enrollmentCount = static_cast<std::uint32_t>(_playerEnrolledActors.size());
    if (!serialization->WriteRecordData(enrollmentCount)) {
        logger::error("Romantasy failed to write player-enrollment identity count");
        return;
    }
    for (const auto identityFormID : _playerEnrolledActors) {
        if (!serialization->WriteRecordData(identityFormID)) {
            logger::error("Romantasy failed to write player-enrollment identity {:08X}", identityFormID);
            return;
        }
    }
    logger::info("Romantasy saved {} player enrollment(s)", enrollmentCount);
}

void RomanceManager::LoadCallback(SKSE::SerializationInterface* serialization)
{
    GetSingleton().Load(serialization);
}

void RomanceManager::RevertCallback(SKSE::SerializationInterface*)
{
    GetSingleton().Revert();
}

void RomanceManager::SaveCallback(SKSE::SerializationInterface* serialization)
{
    GetSingleton().Save(serialization);
}

void RomanceManager::SendRuntimeState(std::string_view message)
{
    if (!RomantasyUI::GetSingleton().IsOpen()) {
        return;
    }

    const auto state = BuildStateJson(message);
    SKSE::GetTaskInterface()->AddTask([state]() {
        RomantasyUI::GetSingleton().SendState(state);
    });
}

RE::BSEventNotifyControl RomanceManager::ProcessEvent(
    const RE::TESTrackedStatsEvent* event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESTrackedStatsEvent>* eventSource)
{
    if (!event || event->stat.empty()) {
        return RE::BSEventNotifyControl::kContinue;
    }

    std::scoped_lock lock(_lock);
    const std::string statName = event->stat.c_str();
    const auto rule = _statRulesByName.find(statName);
    if (rule == _statRulesByName.end()) {
        return RE::BSEventNotifyControl::kContinue;
    }

    const auto currentValue = event->value;
    if (currentValue < 0) {
        return RE::BSEventNotifyControl::kContinue;
    }

    std::int32_t delta = 1;
    if (const auto previous = _lastStatValues.find(statName); previous != _lastStatValues.end()) {
        delta = currentValue - previous->second;
    }
    _lastStatValues[statName] = currentValue;

    if (delta <= 0) {
        return RE::BSEventNotifyControl::kContinue;
    }

    ApplyStatDelta(*rule->second, delta);
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl RomanceManager::ProcessEvent(
    const RE::TESCombatEvent* event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESCombatEvent>* eventSource)
{
    if (!event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    std::scoped_lock lock(_lock);
    if (_pendingLevelChanges.empty()) {
        return RE::BSEventNotifyControl::kContinue;
    }

    const auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || event->actor.get() != player) {
        return RE::BSEventNotifyControl::kContinue;
    }

    if (event->newState == RE::ACTOR_COMBAT_STATE::kNone) {
        FlushPendingLevelChanges();
    }

    return RE::BSEventNotifyControl::kContinue;
}

std::vector<RomanceFollower> RomanceManager::DiscoverFollowers() const
{
    std::vector<RomanceFollower> followers;
    if (!_romanceLevelFaction) {
        logger::warn("ROM_RomanceLevel faction is unavailable; no romanceable NPCs can be discovered");
        return followers;
    }

    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        return followers;
    }

    const auto& npcs = dataHandler->GetFormArray<RE::TESNPC>();
    for (auto* npc : npcs) {
        if (!npc || npc->IsDeleted() || !_authorDefinedBaseForms.contains(npc->GetFormID())) {
            continue;
        }

        if (!npc->IsUnique()) {
            logger::info(
                "Romantasy found non-unique romance definition {} ({:08X}); instances will be tracked by actor reference at runtime",
                SafeString(npc->GetName()),
                npc->GetFormID());
            continue;
        }

        RomanceFollower follower;
        follower.baseFormID = npc->GetFormID();
        follower.editorID = SafeString(npc->GetFormEditorID());
        follower.origin = RomanceProfileOrigin::kAuthorDefined;
        if (auto* actor = npc->GetUniqueActor()) {
            follower.referenceFormID = actor->GetFormID();
            follower.name = DisplayNameForActor(actor, follower);
        } else {
            follower.name = DisplayNameForActor(nullptr, follower);
        }

        std::int8_t romanceRank = 0;
        for (const auto& factionRank : npc->factions) {
            if (factionRank.faction == _romanceLevelFaction) {
                romanceRank = factionRank.rank;
                continue;
            }

            const auto ruleIt = std::ranges::find_if(kStatRules, [&](const auto& rule) {
                const auto foundFaction = _statFactions.find(rule.editorID);
                return foundFaction != _statFactions.end() && foundFaction->second == factionRank.faction;
            });

            if (ruleIt == kStatRules.end()) {
                continue;
            }

            const auto label = LabelForRule(*ruleIt);
            if (factionRank.rank <= 0) {
                follower.statDirections[ruleIt->editorID] = -1;
                follower.dislikes.push_back(label);
            } else {
                follower.statDirections[ruleIt->editorID] = 1;
                follower.likes.push_back(label);
            }
        }

        follower.basePoints = MinimumPointsForFactionRank(romanceRank);
        follower.points = follower.basePoints;
        if (follower.likes.empty() && follower.dislikes.empty()) {
            follower.recent.push_back({ "Awaiting QueryStat preferences", 0 });
        }

        logger::info(
            "Romantasy discovered {} (reference {:08X}, base {:08X}): points={}, likes={}, dislikes={}",
            follower.name,
            follower.referenceFormID,
            follower.baseFormID,
            follower.points,
            follower.likes.size(),
            follower.dislikes.size()
        );
        followers.push_back(std::move(follower));
    }

    std::ranges::sort(followers, [](const auto& lhs, const auto& rhs) {
        return lhs.name < rhs.name;
    });

    return followers;
}
