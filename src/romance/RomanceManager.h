#pragma once

#include <mutex>
#include <unordered_set>

enum class RomanceProfileOrigin : std::uint8_t
{
    kAuthorDefined,
    kExternalManaged,
    kPlayerCreated,
};

[[nodiscard]] constexpr RomanceProfileOrigin ClassifyRomanceProfileOrigin(
    bool authorDefined,
    bool playerEnrolled) noexcept
{
    if (authorDefined) {
        return RomanceProfileOrigin::kAuthorDefined;
    }
    if (playerEnrolled) {
        return RomanceProfileOrigin::kPlayerCreated;
    }
    return RomanceProfileOrigin::kExternalManaged;
}

static_assert(ClassifyRomanceProfileOrigin(true, false) == RomanceProfileOrigin::kAuthorDefined);
static_assert(ClassifyRomanceProfileOrigin(true, true) == RomanceProfileOrigin::kAuthorDefined);
static_assert(ClassifyRomanceProfileOrigin(false, true) == RomanceProfileOrigin::kPlayerCreated);
static_assert(ClassifyRomanceProfileOrigin(false, false) == RomanceProfileOrigin::kExternalManaged);

[[nodiscard]] constexpr bool IsValidAuthorFollowerStartingLevel(std::int32_t level) noexcept
{
    return level >= 1 && level <= 6;
}

[[nodiscard]] constexpr bool IsValidAuthorPreferenceDirection(std::int32_t direction) noexcept
{
    return direction == -1 || direction == 1;
}

static_assert(!IsValidAuthorFollowerStartingLevel(0));
static_assert(IsValidAuthorFollowerStartingLevel(1));
static_assert(IsValidAuthorFollowerStartingLevel(6));
static_assert(!IsValidAuthorFollowerStartingLevel(7));
static_assert(!IsValidAuthorPreferenceDirection(-2));
static_assert(IsValidAuthorPreferenceDirection(-1));
static_assert(!IsValidAuthorPreferenceDirection(0));
static_assert(IsValidAuthorPreferenceDirection(1));
static_assert(!IsValidAuthorPreferenceDirection(2));

struct RomancePointEvent
{
    std::string label;
    std::int32_t points = 0;
    float gameDay = 0.0f;  // in-game days-passed when recorded; 0 = untimed (UI shows no "when")
};

struct RomanceFollower
{
    RE::FormID baseFormID = 0;
    RE::FormID referenceFormID = 0;
    std::string editorID;
    std::string name;
    RomanceProfileOrigin origin = RomanceProfileOrigin::kExternalManaged;
    std::int32_t points = 0;
    std::int32_t basePoints = 0;
    std::unordered_map<std::string_view, std::int32_t> statDirections;
    std::vector<std::string> likes;
    std::vector<std::string> dislikes;
    std::vector<RomancePointEvent> recent;
};

class RomanceManager :
    public RE::BSTEventSink<RE::TESTrackedStatsEvent>,
    public RE::BSTEventSink<RE::TESCombatEvent>
{
public:
    struct StatFactionRule
    {
        std::string_view editorID;
        std::string_view label;
        std::int32_t points;
        RE::FormID fallbackLocalID;
    };

    static RomanceManager& GetSingleton();

    void RegisterSerializationCallbacks(const SKSE::SerializationInterface* serialization);
    void Initialize();
    void DebugAddPoints(std::int32_t points);
    void DebugApplyStat(std::string_view statName, std::int32_t delta);
    bool ModifyPoints(RE::Actor* followerActor, std::int32_t pointsDelta, std::string_view reason, bool showLevelUp);
    bool ApplyPreference(RE::Actor* followerActor, std::string_view statName, std::int32_t delta, bool showLevelUp);
    bool ClearPreferences(RE::Actor* followerActor);
    bool SetPreference(RE::Actor* followerActor, std::string_view statName, std::int32_t direction);
    [[nodiscard]] std::int32_t GetPreference(RE::Actor* followerActor, std::string_view statName);
    bool SetPreferencesManual(RE::Actor* followerActor, bool manual);
    [[nodiscard]] bool IsPreferencesManual(RE::Actor* followerActor);
    bool RegisterAuthorFollower(
        RE::Actor* followerActor,
        const std::vector<std::string>& statNames,
        const std::vector<std::int32_t>& directions,
        std::int32_t startingLevel,
        RE::TESGlobal* relationshipRankMirror);
    bool RegisterAuthorFollowerPointsMirror(
        RE::Actor* followerActor,
        RE::TESGlobal* relationshipPointsMirror);
    bool EnrollPlayerFollower(RE::Actor* followerActor, const std::unordered_map<std::string, std::int32_t>& preferences);
    bool ReplacePlayerPreferences(RE::Actor* followerActor, const std::unordered_map<std::string, std::int32_t>& preferences);
    bool ResetPlayerFollowerPoints(RE::Actor* followerActor);
    bool RemovePlayerFollower(RE::Actor* followerActor);
    [[nodiscard]] bool TryGetPoints(RE::Actor* followerActor, std::int32_t& points);
    [[nodiscard]] std::int32_t GetPoints(RE::Actor* followerActor);
    [[nodiscard]] std::int32_t GetLevel(RE::Actor* followerActor);
    [[nodiscard]] std::string GetLevelName(RE::Actor* followerActor);
    [[nodiscard]] nlohmann::json BuildStateJson(std::string_view message);
    [[nodiscard]] nlohmann::json BuildLevelChangeJson(
        const RomanceFollower& follower,
        std::int32_t oldLevel,
        std::int32_t newLevel,
        std::int32_t pointsDelta) const;

private:
    RomanceManager() = default;
    ~RomanceManager() override = default;

    [[nodiscard]] static std::string_view LevelNameForPoints(std::int32_t points);
    [[nodiscard]] static std::int32_t LevelNumberForPoints(std::int32_t points);
    [[nodiscard]] static std::int32_t MinimumPointsForFactionRank(std::int8_t rank);
    [[nodiscard]] static std::int8_t FactionRankForPoints(std::int32_t points);
    [[nodiscard]] static std::string NextLevelNameForPoints(std::int32_t points);
    [[nodiscard]] static std::string NormalizeStatName(std::string_view statName);
    [[nodiscard]] static std::string SafeString(const char* value);
    [[nodiscard]] static std::string_view ProfileOriginName(RomanceProfileOrigin origin);
    [[nodiscard]] static std::string_view PreferenceCategory(const StatFactionRule& rule);

    [[nodiscard]] nlohmann::json BuildEnrollmentCandidatesJson() const;
    [[nodiscard]] nlohmann::json BuildPreferenceOptionsJson() const;
    [[nodiscard]] RE::Actor* ResolveFollowerActor(const RomanceFollower& follower) const;
    [[nodiscard]] RE::FormID SavedPointsKey(const RomanceFollower& follower) const;
    [[nodiscard]] RE::FormID ActorIdentity(RE::Actor* followerActor) const;
    [[nodiscard]] std::string DisplayNameForActor(RE::Actor* followerActor, const RomanceFollower& follower) const;
    [[nodiscard]] std::string SourcePluginForActor(RE::Actor* followerActor) const;
    [[nodiscard]] RomanceProfileOrigin ProfileOriginForActor(RE::Actor* followerActor) const;
    [[nodiscard]] bool IsAuthorDefinedActor(RE::Actor* followerActor) const;
    [[nodiscard]] bool IsEligibleEnrollmentCandidate(RE::Actor* followerActor) const;
    [[nodiscard]] bool HasAnyPreferenceFaction(RE::Actor* followerActor) const;
    [[nodiscard]] bool IsPreferencesManual(const RomanceFollower& follower) const;
    [[nodiscard]] bool ValidatePreferenceProfile(
        const std::unordered_map<std::string, std::int32_t>& preferences,
        std::vector<std::pair<RE::TESFaction*, std::int32_t>>& resolved) const;
    [[nodiscard]] RomanceFollower* FindFollower(RE::Actor* followerActor);
    [[nodiscard]] RomanceFollower* FindCachedFollower(RE::Actor* followerActor);
    [[nodiscard]] const StatFactionRule* FindStatRule(std::string_view statName) const;
    [[nodiscard]] bool IsActorFollowing(RE::Actor* followerActor) const;
    [[nodiscard]] bool IsFollowerFollowing(const RomanceFollower& follower) const;
    [[nodiscard]] bool IsPlayerInCombat() const;
    [[nodiscard]] std::string LabelForRule(const StatFactionRule& rule) const;
    [[nodiscard]] RE::TESFaction* LookupFaction(
        std::string_view editorID,
        RE::FormID fallbackLocalID,
        std::string_view pluginName) const;
    [[nodiscard]] std::vector<RomanceFollower> DiscoverFollowers() const;
    bool AddPoints(RomanceFollower& follower, std::int32_t pointsDelta, std::string_view reason, bool showLevelUp = true, bool requireFollowing = true);
    void ApplySavedPoints();
    void ApplyStatDelta(const StatFactionRule& rule, std::int32_t delta, bool showLevelUp = true, bool requireFollowing = true);
    void CaptureAuthorDefinedBases();
    void EnsureFollowersDiscovered();
    void FlushPendingLevelChanges();
    void Load(SKSE::SerializationInterface* serialization);
    void MirrorRelationshipLevel(RomanceFollower& follower);
    void MirrorRelationshipLevels();
    void QueueOrShowLevelChange(nlohmann::json payload);
    void RefreshFollowerIdentity(RomanceFollower& follower, RE::Actor* followerActor);
    void RefreshFollowerPreferences(
        RomanceFollower& follower,
        RE::Actor* followerActor,
        const StatFactionRule* rule = nullptr);
    void RefreshLoadedFollowers(const StatFactionRule* rule = nullptr);
    bool RefreshRuntimeFollower(RE::Actor* followerActor, const StatFactionRule* rule = nullptr);
    void RegisterStatsSink();
    void Revert();
    void Save(SKSE::SerializationInterface* serialization) const;
    void SendRuntimeState(std::string_view message);

    static void LoadCallback(SKSE::SerializationInterface* serialization);
    static void RevertCallback(SKSE::SerializationInterface* serialization);
    static void SaveCallback(SKSE::SerializationInterface* serialization);

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESTrackedStatsEvent* event,
        RE::BSTEventSource<RE::TESTrackedStatsEvent>* eventSource) override;
    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESCombatEvent* event,
        RE::BSTEventSource<RE::TESCombatEvent>* eventSource) override;

    RE::TESFaction* _romanceLevelFaction = nullptr;
    RE::TESFaction* _currentFollowerFaction = nullptr;
    std::unordered_map<std::string_view, RE::TESFaction*> _statFactions;
    std::unordered_map<std::string, const StatFactionRule*> _statRulesByName;
    std::unordered_map<std::string, const StatFactionRule*> _statRulesByNormalizedName;
    std::unordered_map<std::string, std::int32_t> _lastStatValues;
    std::unordered_map<RE::FormID, std::int32_t> _savedPoints;
    // Captured once at kDataLoaded, before runtime faction enrollment or mirroring.
    // This is plugin-record provenance and must survive serialization reverts.
    std::unordered_set<RE::FormID> _authorDefinedBaseForms;
    // Reasserted by optional follower integrations after each game load. Kept
    // separate so runtime registration never rewrites plugin-record provenance.
    std::unordered_set<RE::FormID> _authorIntegrationBaseForms;
    std::unordered_map<RE::FormID, RE::TESGlobal*> _relationshipRankMirrors;
    std::unordered_map<RE::FormID, RE::TESGlobal*> _relationshipPointsMirrors;
    std::unordered_set<RE::FormID> _manualPreferenceActors;
    std::unordered_set<RE::FormID> _playerEnrolledActors;
    std::deque<nlohmann::json> _pendingLevelChanges;
    std::vector<RomanceFollower> _followers;
    // Guards _followers/_savedPoints/_lastStatValues/_pendingLevelChanges. Public
    // entry points are reached from the Papyrus VM thread (PapyrusBridge natives)
    // and game event threads as well as the main thread; recursive because locked
    // entry points call back into each other (e.g. ModifyPoints -> BuildStateJson).
    mutable std::recursive_mutex _lock;
    bool _statsSinkRegistered = false;
    bool _combatSinkRegistered = false;
};
