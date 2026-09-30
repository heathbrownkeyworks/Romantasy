#include "romance/SavedRomancePoints.h"
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
    const std::filesystem::path kPlayerProfilesDirectory = "Data/SKSE/Plugins/Romantasy/PlayerProfiles";
    constexpr std::uint32_t kSerializationID = RecordType('R', 'o', 'M', 'a');
    constexpr std::uint32_t kRomancePointsRecord = RecordType('R', 'o', 'P', 't');
    constexpr std::uint32_t kRomancePointsVersion = 2;
    constexpr std::uint32_t kManualPreferencesRecord = RecordType('R', 'o', 'P', 'm');
    constexpr std::uint32_t kManualPreferencesVersion = 1;
    constexpr std::uint32_t kPlayerEnrollmentRecord = RecordType('R', 'o', 'P', 'e');
    constexpr std::uint32_t kPlayerEnrollmentVersion = 1;
    constexpr RE::FormID kRomanceLevelLocalID = 0x800;
    constexpr RE::FormID kCurrentFollowerFactionLocalID = 0x5C84E;

    constexpr const auto& kStatRules = romantasy::StatRules;

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
    LoadFileProfiles();
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
        }
        // Canonical statistic names are available even without the legacy ESP.
        const auto fullLabel = LabelForRule(rule);
        _statRulesByName.emplace(fullLabel, std::addressof(rule));
        _statRulesByName.emplace(std::string(rule.label), std::addressof(rule));
        _statRulesByName.emplace(std::string(rule.editorID), std::addressof(rule));
        _statRulesByNormalizedName.emplace(NormalizeStatName(fullLabel), std::addressof(rule));
        _statRulesByNormalizedName.emplace(NormalizeStatName(rule.label), std::addressof(rule));
        _statRulesByNormalizedName.emplace(NormalizeStatName(rule.editorID), std::addressof(rule));
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
    logger::info("Romantasy debug adding {} points to active followers ({} registered)", points, _followers.size());
    bool changedAnyFollower = false;
    for (auto& follower : _followers) {
        changedAnyFollower = AddPoints(follower, points, "Debug adjustment") || changedAnyFollower;
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
    ApplyStatDelta(*rule, delta);
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
    if (follower->origin == RomanceProfileOrigin::kPlayerCreated) {
        return ReplacePlayerPreferences(followerActor, {});
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
    if (follower->origin == RomanceProfileOrigin::kPlayerCreated) {
        std::unordered_map<std::string, std::int32_t> preferences;
        for (const auto& [name, value] : follower->statDirections) preferences.emplace(name, value);
        preferences.erase(std::string(rule->editorID));
        if (direction != 0) preferences.emplace(rule->editorID, direction < 0 ? -1 : 1);
        return ReplacePlayerPreferences(followerActor, preferences);
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
    if (_fileProfiles.contains(baseNpc->GetFormID())) {
        logger::warn("Romantasy author registration rejected; {} is defined by a TOML profile", followerActor->GetFormID());
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

    const auto identityFormID = ActorIdentity(followerActor);
    auto* baseNpc = followerActor->GetActorBase();
    if (identityFormID == 0 || !baseNpc || !StorePlayerProfile(followerActor, preferences)) {
        return false;
    }

    _savedPoints[identityFormID] = 0;

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
        _fileProfiles.at(baseNpc->GetFormID()).preferences.size());
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

    if (!StorePlayerProfile(followerActor, preferences)) {
        return false;
    }

    // All instances use the NPC's personality, but retain their own points.
    for (auto& entry : _followers) {
        if (entry.baseFormID == follower->baseFormID) {
            entry.origin = RomanceProfileOrigin::kPlayerCreated;
            ApplyFilePreferences(entry, _fileProfiles.at(entry.baseFormID));
        }
    }
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
    if (!StorePlayerProfile(followerActor, {}, false)) {
        return false;
    }

    _savedPoints.erase(identityFormID);
    _savedPoints.erase(baseFormID);
    _manualPreferenceActors.erase(identityFormID);
    _playerEnrolledActors.erase(identityFormID);
    std::erase_if(_followers, [&](const auto& entry) {
        if (entry.baseFormID != baseFormID) return false;
        _savedPoints.erase(SavedPointsKey(entry));
        _manualPreferenceActors.erase(SavedPointsKey(entry));
        _playerEnrolledActors.erase(SavedPointsKey(entry));
        if (auto* actor = ResolveFollowerActor(entry)) ClearLegacyPlayerFactions(actor);
        return true;
    });

    logger::info("Romantasy removed player-created bond {} ({:08X})", followerName, identityFormID);
    SendRuntimeState("Player-created bond removed.");
    return true;
}

std::optional<std::int32_t> RomanceManager::QueryPoints(RE::Actor* followerActor) const
{
    std::scoped_lock lock(_lock);
    auto* baseNpc = followerActor ? followerActor->GetActorBase() : nullptr;
    if (!baseNpc || followerActor == RE::PlayerCharacter::GetSingleton()) {
        return std::nullopt;
    }
    const auto reference = followerActor->GetFormID();
    const auto profile = _fileProfiles.find(baseNpc->GetFormID());
    if (profile != _fileProfiles.end() && !profile->second.enabled) return std::nullopt;
    for (const auto& follower : _followers) {
        if (follower.referenceFormID != 0 && follower.referenceFormID == reference) {
            return follower.points;
        }
    }
    if (profile != _fileProfiles.end()) {
        return SavedOrInitialPoints(reference, baseNpc->GetFormID(), (profile->second.startingLevel - 1) * 500);
    }
    return std::nullopt;
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

        const auto profile = _fileProfiles.find(follower.baseFormID);
        const std::string profileFile = profile == _fileProfiles.end() ? std::string{} :
            std::filesystem::path(profile->second.source).filename().string();
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
            { "profileFile", profileFile },
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

void RomanceManager::LoadFileProfiles()
{
    _fileProfiles.clear();
    auto result = romantasy::LoadProfiles("Data/SKSE/Plugins/Romantasy/Profiles");
    auto playerResult = romantasy::LoadProfiles(kPlayerProfilesDirectory, true);
    std::ranges::move(playerResult.profiles, std::back_inserter(result.profiles));
    std::ranges::move(playerResult.errors, std::back_inserter(result.errors));
    for (const auto& error : result.errors) {
        logger::error("Romantasy profile: {}", error);
    }
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        logger::error("Romantasy cannot resolve file profiles; data handler unavailable");
        return;
    }
    for (auto& profile : result.profiles) {
        const auto* file = dataHandler->LookupModByName(profile.plugin);
        // LookupForm masks ESL IDs; reject oversized local IDs before resolution.
        if (!file || (file->IsLight() && profile.localID > 0xFFF)) {
            logger::error("Romantasy profile {}: plugin unavailable or local ID outside its range", profile.source);
            continue;
        }
        auto* npc = dataHandler->LookupForm<RE::TESNPC>(profile.localID, profile.plugin);
        if (!npc || npc->IsDeleted() || npc->GetFormID() == 7 || (npc->GetRace() && npc->GetRace()->IsChildRace())) {
            logger::error("Romantasy profile {}: NPC is unavailable, deleted, a child, or the player", profile.source);
            continue;
        }
        if (profile.playerOwned && (_fileProfiles.contains(npc->GetFormID()) ||
                (_romanceLevelFaction && npc->IsInFaction(_romanceLevelFaction)))) {
            logger::warn("Romantasy ignored player profile {}; a mod owns this NPC's romance", profile.source);
            continue;
        }
        logger::info("Romantasy loaded profile {} for {:08X}", profile.source, npc->GetFormID());
        _fileProfiles.emplace(npc->GetFormID(), std::move(profile));
    }
    logger::info("Romantasy loaded {} file-defined NPC profile(s)", _fileProfiles.size());
}

void RomanceManager::ApplyFilePreferences(RomanceFollower& follower, const romantasy::NpcProfile& profile) const
{
    follower.statDirections.clear();
    follower.likes.clear();
    follower.dislikes.clear();
    for (const auto& rule : kStatRules) {
        const auto preference = profile.preferences.find(std::string(rule.editorID));
        if (preference == profile.preferences.end()) {
            continue;
        }
        // String views refer to the static statistic table, never temporary TOML strings.
        follower.statDirections.emplace(rule.editorID, preference->second);
        (preference->second < 0 ? follower.dislikes : follower.likes).push_back(LabelForRule(rule));
    }
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
        if (npc && !npc->IsDeleted() && !_fileProfiles.contains(npc->GetFormID()) && npc->IsInFaction(_romanceLevelFaction)) {
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
    if (!baseNpc) return false;
    const auto profile = _fileProfiles.find(baseNpc->GetFormID());
    return (profile != _fileProfiles.end() && !profile->second.playerOwned) ||
        _authorDefinedBaseForms.contains(baseNpc->GetFormID()) ||
        _authorIntegrationBaseForms.contains(baseNpc->GetFormID());
}

RomanceProfileOrigin RomanceManager::ProfileOriginForActor(RE::Actor* followerActor) const
{
    const auto identityFormID = ActorIdentity(followerActor);
    auto* baseNpc = followerActor ? followerActor->GetActorBase() : nullptr;
    if (baseNpc) {
        const auto profile = _fileProfiles.find(baseNpc->GetFormID());
        if (profile != _fileProfiles.end()) {
            return profile->second.playerOwned ? RomanceProfileOrigin::kPlayerCreated : RomanceProfileOrigin::kAuthorDefined;
        }
    }
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
        !IsActorFollowing(followerActor)) {
        return false;
    }
    auto* baseNpc = followerActor->GetActorBase();
    if (baseNpc->IsDynamicForm() || !baseNpc->GetFile(0) || !baseNpc->GetLocalFormID()) return false;
    if (const auto profile = _fileProfiles.find(baseNpc->GetFormID()); profile != _fileProfiles.end()) {
        return profile->second.playerOwned && !profile->second.enabled;
    }
    if (IsAuthorDefinedActor(followerActor) ||
        (_romanceLevelFaction && followerActor->IsInFaction(_romanceLevelFaction)) || HasAnyPreferenceFaction(followerActor)) {
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
    std::unordered_map<std::string, std::int32_t>& resolved) const
{
    resolved.clear();
    resolved.reserve(preferences.size());
    std::unordered_set<std::string_view> seen;
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
        if (!seen.insert(rule->editorID).second) {
            logger::warn("Romantasy preference profile specified {} more than once", rule->editorID);
            return false;
        }
        if (direction != 0) resolved.emplace(std::string(rule->editorID), direction);
    }
    return true;
}

bool RomanceManager::StorePlayerProfile(
    RE::Actor* actor, const std::unordered_map<std::string, std::int32_t>& preferences, bool enabled)
{
    auto* npc = actor ? actor->GetActorBase() : nullptr;
    if (!npc || npc->IsDynamicForm() || !npc->GetFile(0) || IsAuthorDefinedActor(actor)) return false;
    romantasy::NpcProfile profile;
    if (const auto existing = _fileProfiles.find(npc->GetFormID()); existing != _fileProfiles.end()) {
        profile = existing->second;
        if (!profile.playerOwned) return false;
    }
    if (!ValidatePreferenceProfile(preferences, profile.preferences)) return false;
    profile.plugin = SourcePluginForActor(actor);
    profile.localID = npc->GetLocalFormID();
    profile.playerOwned = true;
    profile.enabled = enabled;
    try {
        // No runtime changes until the complete file has been committed.
        auto saved = romantasy::SavePlayerProfile(kPlayerProfilesDirectory, profile);
        logger::info("Romantasy saved player profile {} (enabled={})", saved.source, saved.enabled);
        _fileProfiles.insert_or_assign(npc->GetFormID(), std::move(saved));
    } catch (const std::exception& e) {
        logger::error("Romantasy could not save player profile: {}", e.what());
        return false;
    }
    ClearLegacyPlayerFactions(actor);
    _playerEnrolledActors.erase(ActorIdentity(actor));
    _playerEnrolledActors.erase(npc->GetFormID());
    _manualPreferenceActors.erase(ActorIdentity(actor));
    _manualPreferenceActors.erase(npc->GetFormID());
    return true;
}

void RomanceManager::ClearLegacyPlayerFactions(RE::Actor* actor)
{
    if (!actor) return;
    for (const auto& [editorID, faction] : _statFactions) {
        if (faction && actor->IsInFaction(faction)) actor->RemoveFromFaction(faction);
    }
    if (_romanceLevelFaction && actor->IsInFaction(_romanceLevelFaction)) actor->RemoveFromFaction(_romanceLevelFaction);
}

void RomanceManager::MigratePlayerProfile(RE::Actor* actor)
{
    auto* npc = actor ? actor->GetActorBase() : nullptr;
    if (!npc || IsAuthorDefinedActor(actor)) return;
    if (const auto profile = _fileProfiles.find(npc->GetFormID()); profile != _fileProfiles.end()) {
        if (profile->second.playerOwned) {
            // Old saves may still contain the factions even after conversion/removal.
            ClearLegacyPlayerFactions(actor);
            _playerEnrolledActors.erase(ActorIdentity(actor));
            _playerEnrolledActors.erase(npc->GetFormID());
        }
        return;
    }
    if (!_playerEnrolledActors.contains(ActorIdentity(actor)) && !_playerEnrolledActors.contains(npc->GetFormID())) return;
    std::unordered_map<std::string, std::int32_t> preferences;
    for (const auto& [editorID, faction] : _statFactions) {
        if (faction && actor->IsInFaction(faction)) {
            preferences.emplace(editorID, actor->GetFactionRank(faction, false) <= 0 ? -1 : 1);
        }
    }
    const auto initial = _romanceLevelFaction ? MinimumPointsForFactionRank(
        static_cast<std::int8_t>(actor->GetFactionRank(_romanceLevelFaction, false))) : 0;
    const auto points = SavedOrInitialPoints(ActorIdentity(actor), npc->GetFormID(), initial);
    if (StorePlayerProfile(actor, preferences)) {
        _savedPoints.try_emplace(ActorIdentity(actor), points);
        logger::info("Romantasy migrated legacy player bond {:08X}; retained {} points", ActorIdentity(actor), points);
    }
}

bool RomanceManager::IsPreferencesManual(const RomanceFollower& follower) const
{
    if (const auto profile = _fileProfiles.find(follower.baseFormID); profile != _fileProfiles.end()) {
        return profile->second.playerOwned;
    }
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
    if (const auto profile = _fileProfiles.find(follower.baseFormID); profile != _fileProfiles.end()) {
        ApplyFilePreferences(follower, profile->second);
        return;
    }

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
    auto* baseNpc = followerActor ? followerActor->GetActorBase() : nullptr;
    if (!baseNpc) {
        return false;
    }
    MigratePlayerProfile(followerActor);
    const auto profile = _fileProfiles.find(baseNpc->GetFormID());
    if (profile != _fileProfiles.end() && !profile->second.enabled) return false;
    if (profile == _fileProfiles.end() &&
        (!_romanceLevelFaction || !followerActor->IsInFaction(_romanceLevelFaction))) {
        return false;
    }

    if (auto* follower = FindCachedFollower(followerActor)) {
        follower->origin = ProfileOriginForActor(followerActor);
        RefreshFollowerIdentity(*follower, followerActor);
        RefreshFollowerPreferences(*follower, followerActor, rule);
        return false;
    }

    RomanceFollower follower;
    follower.baseFormID = baseNpc->GetFormID();
    follower.referenceFormID = followerActor->GetFormID();
    follower.editorID = SafeString(baseNpc->GetFormEditorID());
    follower.name = DisplayNameForActor(followerActor, follower);
    follower.origin = ProfileOriginForActor(followerActor);

    if (profile != _fileProfiles.end()) {
        follower.basePoints = (profile->second.startingLevel - 1) * 500;
    } else {
        const auto romanceRank = static_cast<std::int8_t>(followerActor->GetFactionRank(_romanceLevelFaction, false));
        follower.basePoints = MinimumPointsForFactionRank(romanceRank);
    }
    follower.points = SavedOrInitialPoints(follower.referenceFormID, follower.baseFormID, follower.basePoints);

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

std::int32_t RomanceManager::SavedOrInitialPoints(RE::FormID reference, RE::FormID base, std::int32_t initial) const
{
    // Base-keyed saves can migrate only to unique file-defined NPCs, never to
    // separate spawned instances that happen to share the same base record.
    auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(base);
    const bool allowLegacyBase = !_fileProfiles.contains(base) || (npc && npc->IsUnique());
    return romantasy::RestorePoints(_savedPoints, reference, base, allowLegacyBase, initial);
}

void RomanceManager::ApplySavedPoints()
{
    for (auto& follower : _followers) {
        follower.points = SavedOrInitialPoints(follower.referenceFormID, follower.baseFormID, follower.basePoints);
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

    // Snapshot because successful migration consumes the legacy enrollment marker.
    const std::vector<RE::FormID> legacyEnrollments(_playerEnrolledActors.begin(), _playerEnrolledActors.end());
    for (const auto identityFormID : legacyEnrollments) {
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(identityFormID);
        if (!actor) {
            if (auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(identityFormID)) actor = npc->GetUniqueActor();
        }
        if (!actor) {
            continue;
        }
        if (IsAuthorDefinedActor(actor)) {
            _playerEnrolledActors.erase(identityFormID);
            _manualPreferenceActors.erase(identityFormID);
            logger::warn(
                "Romantasy ignored player-enrollment marker {:08X}; the follower is author-defined",
                identityFormID);
            continue;
        }
        MigratePlayerProfile(actor);
    }
    _followers = DiscoverFollowers();
    for (const auto& follower : _followers) {
        if (follower.origin == RomanceProfileOrigin::kAuthorDefined) {
            _manualPreferenceActors.erase(SavedPointsKey(follower));
            _manualPreferenceActors.erase(follower.baseFormID);
        }
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
    if (_fileProfiles.contains(follower.baseFormID) || !_romanceLevelFaction) {
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
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        return followers;
    }

    const auto& npcs = dataHandler->GetFormArray<RE::TESNPC>();
    for (auto* npc : npcs) {
        if (!npc || npc->IsDeleted() ||
            (!_fileProfiles.contains(npc->GetFormID()) && !_authorDefinedBaseForms.contains(npc->GetFormID()))) {
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

        if (const auto profile = _fileProfiles.find(npc->GetFormID()); profile != _fileProfiles.end()) {
            if (!profile->second.enabled) continue;
            follower.origin = profile->second.playerOwned ? RomanceProfileOrigin::kPlayerCreated : RomanceProfileOrigin::kAuthorDefined;
            follower.basePoints = (profile->second.startingLevel - 1) * 500;
            ApplyFilePreferences(follower, profile->second);
        } else {
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
        }
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
