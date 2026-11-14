#pragma once

#include "romance/OutboxLock.h"
#include "romance/Pacing.h"
#include "romance/PointsView.h"
#include "romance/RomanceStats.h"
#include "romance/RomanceProfiles.h"
#include "romance/SaveCodec.h"
#include "romance/Throttle.h"
#include "romance/Why.h"
#include "skyrimnet/ConversationLimits.h"
#include "ui/Whispers.h"

#include <atomic>
#include <memory>
#include <optional>
#include <unordered_set>

namespace romantasy::health
{
    struct Facts;
}

namespace romantasy::ostim
{
    struct SceneEnd;
}

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
    std::vector<std::string> favorites;  // weight 2; each label is also in likes
    std::vector<std::string> petPeeves;  // weight -2; each label is also in dislikes
    romantasy::ostim::Welcome moments;   // from the follower's file; defaults otherwise
    std::vector<RomancePointEvent> recent;
};

class RomanceManager :
    public RE::BSTEventSink<RE::TESTrackedStatsEvent>,
    public RE::BSTEventSink<RE::TESCombatEvent>,
    public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
    public RE::BSTEventSink<RE::TESActorLocationChangeEvent>,
    public RE::BSTEventSink<RE::TESSleepStopEvent>,
    public RE::BSTEventSink<RE::TESWaitStopEvent>
{
public:
    using StatFactionRule = romantasy::StatRule;

    static RomanceManager& GetSingleton();

    void RegisterSerializationCallbacks(const SKSE::SerializationInterface* serialization);
    void Initialize();
    void DebugAddPoints(std::int32_t points);
    void DebugApplyStat(std::string_view statName, std::int32_t delta);
    // Author test functions (Papyrus checks author mode first). Each returns false, or an empty
    // string, for an actor that is not a bond.
    // One built-in deed for one follower as a real statistic event would score it: paced and gated
    // as a player would get it, with only the following requirement off. True when the total moved.
    bool DebugDeed(RE::Actor* followerActor, std::string_view statistic, std::int32_t count);
    // Sets the total through AddPoints (not gated), so the journal, mood and announcements behave as usual.
    bool DebugSetPoints(RE::Actor* followerActor, std::int32_t points);
    bool DebugSetDaysTogether(RE::Actor* followerActor, float days);
    bool DebugSetMood(RE::Actor* followerActor, std::int32_t score);
    // Releases every waiting tier change as a calm moment (a rest) would. True when none is left waiting.
    bool DebugAnnounceNow();
    [[nodiscard]] std::string DebugDescribe(RE::Actor* followerActor);
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
    // Condition evaluation must never discover actors or mutate romance state.
    [[nodiscard]] std::optional<std::int32_t> QueryPoints(RE::Actor* followerActor) const;
    // Mood for conditions and Papyrus, without the lock: 0 unmanaged, 1 Strained, 2 Settled, 3 Warm.
    [[nodiscard]] std::int32_t QueryMood(RE::Actor* followerActor) const;
    // The SkyrimNet bond line for an actor (Papyrus VM thread), or "" without a switched-on bond, a usable SkyrimNet,
    // or the bridge setting. Reads under the lock and changes nothing.
    [[nodiscard]] std::string SkyrimNetBondLine(RE::Actor* followerActor) const;
    // SkyrimNet asks whether an actor may take RomantasyTouched (touched) or RomantasyHurt (Papyrus VM thread): true
    // only with a usable SkyrimNet, both SkyrimNet settings on, a switched-on bond and the conversation limits allowing
    // it now. Reads under the lock and changes nothing.
    [[nodiscard]] bool ConversationEligible(RE::Actor* followerActor, bool touched) const;
    // SkyrimNet ran one of those actions: checks everything ConversationEligible checks again, then applies +5
    // "A kind word" or -5 "A harsh word" (not paced, following not required, Slow burn's gate still holds the gain).
    // True when AddPoints applied it.
    bool ConversationNudge(RE::Actor* followerActor, bool touched);
    // The Dialogue menu closed (recruiting and dismissing happen there) or the Sleep/Wait menu
    // opened (before the hours pass): look at who is following.
    void OnDialogueClosed();
    // Game thread: the player's OStim scene stopped or was lost. A stopped scene is judged as a shared moment for each
    // switched-on bond in it; then popups held for the scene show.
    void OnSceneEnded(const romantasy::ostim::SceneEnd& scene);
    // Papyrus VM thread. Read under the lock, changing nothing: 0 and -1.0 for an actor that is not a bond or has none.
    [[nodiscard]] std::int32_t GetMomentCount(RE::Actor* follower);
    [[nodiscard]] float GetDaysSinceMoment(RE::Actor* follower);
    // Author test function: judges a moment of `kind` ("affection", "closeness", "intimacy") as a scene's end would.
    // Works without OStim; following not required. True when the bond's total moved.
    bool DebugSharedMoment(RE::Actor* follower, std::string_view kind, float seconds, bool others, bool force);
    // Papyrus VM thread. Starts a shared moment of `kind` ("affection", "closeness", "intimacy", or "" for the deepest
    // the bond welcomes now) and answers with a result code (romantasy::ostim::StartCode): 0 when the start is claimed
    // and handed to the game thread. Codes 3 and 4 whisper and send Romantasy_OnMomentRefused. A null actor or the
    // player is code 2. Following is not required.
    std::int32_t TryShareMoment(RE::Actor* follower, std::string_view kind);
    // The same answer, changing nothing.
    [[nodiscard]] std::int32_t CanShareMoment(RE::Actor* follower, std::string_view kind) const;
    // Game thread: a moment Romantasy asked for could not start, or OStim went somewhere else and it was stopped:
    // sends Romantasy_OnMomentRefused with code 6. No whisper.
    void OnMomentCouldNotStart(RE::FormID follower, romantasy::ostim::Kind kind);
    // A save has finished loading (kPostLoadGame): re-seed damaged clock values, then look at who is following.
    void OnGameLoaded();
    [[nodiscard]] std::int32_t GetPoints(RE::Actor* followerActor);
    [[nodiscard]] std::int32_t GetLevel(RE::Actor* followerActor);
    [[nodiscard]] std::string GetLevelName(RE::Actor* followerActor);
    [[nodiscard]] nlohmann::json BuildStateJson(std::string_view message);
    // The romantasyJournal message for the bond a page identity names (hex FormID text, resolved as a
    // bond request's is): found false when it names no bond; a bond with no journal yet has empty lists.
    [[nodiscard]] nlohmann::json BuildJournalJson(std::string_view identity);
    [[nodiscard]] nlohmann::json BuildLevelChangeJson(
        const RomanceFollower& follower,
        std::int32_t oldLevel,
        std::int32_t newLevel,
        std::int32_t pointsDelta) const;
    // The health facts the manager owns: its factions, the save's state, the bonds and the rejected personality files.
    void FillHealth(romantasy::health::Facts& facts) const;

private:
    // A bond's short-term mood and its company record, kept beside _savedPoints under the same key.
    struct BondMood
    {
        romantasy::mood::Mood mood;
        romantasy::mood::Company company;
    };

    RomanceManager();
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
        std::unordered_map<std::string, std::int32_t>& resolved) const;
    bool StorePlayerProfile(RE::Actor* actor, const std::unordered_map<std::string, std::int32_t>& preferences, bool enabled = true);
    void MigratePlayerProfile(RE::Actor* actor);
    void ClearLegacyPlayerFactions(RE::Actor* actor);
    [[nodiscard]] RomanceFollower* FindFollower(RE::Actor* followerActor);
    [[nodiscard]] RomanceFollower* FindCachedFollower(RE::Actor* followerActor);
    // The cached bond for an actor, changing nothing: the reference first, then a unique base that has no reference.
    [[nodiscard]] const RomanceFollower* MatchCachedFollower(RE::Actor* followerActor) const;
    // No personality file, or an enabled one.
    [[nodiscard]] bool IsBondSwitchedOn(const RomanceFollower& follower) const;
    [[nodiscard]] const StatFactionRule* FindStatRule(std::string_view statName) const;
    [[nodiscard]] bool IsActorFollowing(RE::Actor* followerActor) const;
    [[nodiscard]] bool IsFollowerFollowing(const RomanceFollower& follower) const;
    [[nodiscard]] bool IsPlayerInCombat() const;
    // The player is in an OStim scene and the hold setting is on: tier changes wait.
    [[nodiscard]] bool SceneHolds() const;
    // Under the lock: the answer to starting a moment with this actor, and the switched-on cached bond it is for
    // (nullptr when there is none). The actor is neither null nor the player.
    [[nodiscard]] romantasy::ostim::StartAnswer CheckMomentStart(
        RE::Actor* followerActor, std::string_view kind, const RomanceFollower*& follower) const;
    [[nodiscard]] std::string LabelForRule(const StatFactionRule& rule) const;
    [[nodiscard]] RE::TESFaction* LookupFaction(
        std::string_view editorID,
        RE::FormID fallbackLocalID,
        std::string_view pluginName) const;
    [[nodiscard]] std::vector<RomanceFollower> DiscoverFollowers() const;
    [[nodiscard]] BondMood& MoodFor(const RomanceFollower& follower);
    [[nodiscard]] bool HasMood(const RomanceFollower& follower) const;
    [[nodiscard]] romantasy::mood::Mood MoodOrSettled(const RomanceFollower& follower) const;
    // The bond's journal, created when missing; one stored under the base moves to the reference, as moods do.
    [[nodiscard]] romantasy::journal::Journal& JournalFor(const RomanceFollower& follower);
    // The bond's shared moments, created when missing; one stored under the base moves to the reference, as the journal does.
    [[nodiscard]] romantasy::ostim::MomentState& MomentsFor(const RomanceFollower& follower);
    // The same entry found without creating or moving one; nullptr when there is none.
    [[nodiscard]] const romantasy::ostim::MomentState* FindMoments(const RomanceFollower& follower) const;
    void ObserveCompany();
    // Days the bond has spent following, read without creating a mood entry; 0 when it has none.
    [[nodiscard]] float DaysTogether(const RomanceFollower& follower) const;
    // `gated` is false only for the ledger's developer tools and DebugSetPoints, which Slow burn's gates ignore.
    bool AddPoints(
        RomanceFollower& follower, std::int32_t pointsDelta, std::string_view reason, bool showLevelUp = true,
        bool requireFollowing = true, bool gated = true);
    // Judges one shared moment for one bond and counts it: notes the day, adds its points (gated, never paced), sends
    // Romantasy_OnSharedMoment. True when the total moved. Ends with SendRuntimeState, which may reorder _followers.
    bool AwardMoment(RomanceFollower& follower, const romantasy::ostim::MomentFacts& facts);
    void ApplySavedPoints();
    // The why-line for a decision that changed nothing (detailed log only). No deed: a script award.
    // The gate a why line names for a gain Slow burn's gate held: its tier name, the days so far and the days needed.
    struct WhyGate
    {
        std::string_view tier;
        float days = 0.0f;
        std::int32_t needed = 0;
    };
    [[nodiscard]] WhyGate GateForWhy(const RomanceFollower& follower) const;
    void LogWhyNothing(
        const RomanceFollower& follower, std::optional<std::string_view> deed, std::int32_t count,
        romantasy::why::Nothing reason) const;
    // True when any follower's points changed. A non-zero `onlyKey` scores only the follower with that
    // SavedPointsKey and skips the walk for new followers (DebugDeed).
    bool ApplyStatDelta(
        const StatFactionRule& rule, std::int32_t delta, bool showLevelUp = true, bool requireFollowing = true,
        bool gated = true, RE::FormID onlyKey = 0);
    // The popup and the gain or loss event for one tier change, sent now.
    // `whisper`: false when a developer tool made the change (kept in the hold record while it waits).
    void AnnounceLevelChange(
        const RomanceFollower& follower, std::int32_t oldLevel, std::int32_t newLevel, std::int32_t pointsDelta, bool whisper);
    // Merges a tier change into the one waiting for this bond, then announces it at once or
    // keeps it for a calm moment, as the Tier changes arrive setting says.
    void HoldOrAnnounceLevelChange(const RomanceFollower& follower, std::int32_t oldLevel, std::int32_t newLevel, std::int32_t pointsDelta);
    [[nodiscard]] std::optional<romantasy::pacing::PendingChange> TakeWaitingTierChange(const RomanceFollower& follower);
    [[nodiscard]] const RomanceFollower* FindFollowerForKey(RE::FormID key) const;
    // Base-keyed state may move to this follower's reference (a unique NPC, or one without a personality file).
    [[nodiscard]] bool AllowsLegacyBase(const RomanceFollower& follower) const;
    // Null-safe: no player, no location or no keyword means not a calm place.
    [[nodiscard]] bool IsCalmPlace(std::optional<const RE::BGSLocation*> arrivedAt) const;
    // Announces every waiting tier change whose moment has come. `arrivedAt` holds the location
    // the player has just entered (null: the wilds, never calm); empty means the player's current location.
    void ReleaseTierChanges(bool justRested, std::optional<const RE::BGSLocation*> arrivedAt = std::nullopt);
    void CaptureAuthorDefinedBases();
    void LoadFileProfiles();
    [[nodiscard]] RE::TESNPC* ResolveProfileNpc(
        std::string_view plugin, std::uint32_t localID, std::string_view source, std::string* failure = nullptr,
        bool* pluginUnavailable = nullptr, bool* pluginNotLoaded = nullptr) const;
    void UseLibraryBonds();
    void ApplyPlayerBonds(
        const romantasy::save::ReadResult<romantasy::save::PlayerBond>& bonds,
        SKSE::SerializationInterface* serialization);
    void ApplyFilePreferences(RomanceFollower& follower, const romantasy::NpcProfile& profile) const;
    [[nodiscard]] std::int32_t SavedOrInitialPoints(RE::FormID reference, RE::FormID base, std::int32_t initial) const;
    [[nodiscard]] std::shared_ptr<const romantasy::PointsView> BuildPointsView() const;
    void EnsureFollowersDiscovered();
    void FlushPendingLevelChanges();
    void Load(SKSE::SerializationInterface* serialization);
    [[nodiscard]] std::size_t KeptBondsCount() const;
    void MirrorRelationshipLevel(RomanceFollower& follower);
    void MirrorRelationshipLevels();
    // `whisper`: a tier whisper may stand in for the popup when its direction is switched off.
    void QueueOrShowLevelChange(nlohmann::json payload, bool whisper);
    // Hands a whisper to the UI through the outbox, unless whispers are off or a developer tool made the change.
    void RaiseWhisper(romantasy::ui::whispers::Whisper whisper);
    void RefreshFollowerIdentity(RomanceFollower& follower, RE::Actor* followerActor);
    void RefreshFollowerPreferences(
        RomanceFollower& follower,
        RE::Actor* followerActor,
        const StatFactionRule* rule = nullptr);
    void RefreshLoadedFollowers(const StatFactionRule* rule = nullptr);
    bool RefreshRuntimeFollower(RE::Actor* followerActor, const StatFactionRule* rule = nullptr);
    void RegisterStatsSink();
    void Revert();
    [[nodiscard]] bool Save(SKSE::SerializationInterface* serialization) const;
    void ClearLoadDamaged();
    bool SavePoints(SKSE::SerializationInterface* serialization) const;
    bool SaveMoods(SKSE::SerializationInterface* serialization) const;
    bool SaveManualPreferences(SKSE::SerializationInterface* serialization) const;
    bool SavePlayerEnrollments(SKSE::SerializationInterface* serialization) const;
    bool SavePlayerBonds(SKSE::SerializationInterface* serialization) const;
    bool SavePending(SKSE::SerializationInterface* serialization) const;
    bool SaveJournals(SKSE::SerializationInterface* serialization) const;
    bool SaveMoments(SKSE::SerializationInterface* serialization) const;
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
    RE::BSEventNotifyControl ProcessEvent(
        const RE::MenuOpenCloseEvent* event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>* eventSource) override;
    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESActorLocationChangeEvent* event,
        RE::BSTEventSource<RE::TESActorLocationChangeEvent>* eventSource) override;
    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESSleepStopEvent* event,
        RE::BSTEventSource<RE::TESSleepStopEvent>* eventSource) override;
    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESWaitStopEvent* event,
        RE::BSTEventSource<RE::TESWaitStopEvent>* eventSource) override;

    // Effective profiles every reader uses: the author profiles plus this save's player
    // bonds. The player part is save state: Revert resets it to the library on disk, and a
    // save's RoPb record replaces it. Earned points remain in the cosave.
    std::unordered_map<RE::FormID, romantasy::NpcProfile> _fileProfiles;
    // Author (mod-owned) TOML profiles, resolved once per session.
    std::unordered_map<RE::FormID, romantasy::NpcProfile> _authorProfiles;
    // Player TOML files on disk, resolved once per session and refreshed by every file
    // write; the starting bonds for a new game or a save without the RoPb record.
    std::unordered_map<RE::FormID, romantasy::NpcProfile> _playerLibrary;
    // Player TOML files on disk whose plugin is not loaded, kept as save bonds so a save that
    // starts from the library writes them back. Session state like _playerLibrary.
    std::vector<romantasy::save::PlayerBond> _libraryOrphans;
    // Player bonds whose NPC did not resolve or is now mod-owned; written back verbatim by every Save.
    std::vector<romantasy::save::PlayerBond> _orphanedBonds;
    // "<file name>: <reason>" for each personality file LoadFileProfiles could not use; for the health report.
    std::vector<std::string> _profileProblems;
    RE::TESFaction* _romanceLevelFaction = nullptr;
    RE::TESFaction* _currentFollowerFaction = nullptr;
    std::unordered_map<std::string_view, RE::TESFaction*> _statFactions;
    std::unordered_map<std::string, const StatFactionRule*> _statRulesByName;
    std::unordered_map<std::string, const StatFactionRule*> _statRulesByNormalizedName;
    std::unordered_map<std::string, std::int32_t> _lastStatValues;
    std::unordered_map<RE::FormID, std::int32_t> _savedPoints;
    // Points whose follower's plugin is not loaded; written back verbatim by every Save.
    std::vector<romantasy::save::PointsEntry> _orphanedPoints;
    // Mood and company per bond, keyed by SavedPointsKey like _savedPoints.
    std::unordered_map<RE::FormID, BondMood> _moods;
    // Moods whose follower's plugin is not loaded; written back verbatim by every Save.
    std::vector<romantasy::save::MoodEntry> _orphanedMoods;
    // Tier changes waiting for a calm moment, keyed by SavedPointsKey like _savedPoints. Saved as RoPn.
    std::unordered_map<RE::FormID, romantasy::pacing::PendingChange> _waitingTierChanges;
    // Kept in step with _waitingTierChanges so the combat and location sinks can skip the lock.
    std::atomic_bool _hasWaitingTierChanges = false;
    // The journal per bond, keyed by SavedPointsKey like _savedPoints. Saved as RoJn. Only records.
    std::unordered_map<RE::FormID, romantasy::journal::Journal> _journals;
    // Journals whose follower's plugin is not loaded; normalised on load, then kept and written back by every Save.
    std::vector<romantasy::save::JournalEntry> _orphanedJournals;
    // Shared moments per bond, keyed by SavedPointsKey like _savedPoints. Saved as RoMo.
    std::unordered_map<RE::FormID, romantasy::ostim::MomentState> _moments;
    // Moments whose follower's plugin is not loaded; kept and written back by every Save.
    std::vector<romantasy::save::MomentEntry> _orphanedMoments;
    // How often conversation has moved each bond, keyed like _savedPoints. Memory only: cleared by Load and Revert.
    std::unordered_map<RE::FormID, romantasy::skyrimnet::ConversationState> _conversations;
    // Bonds carrying half a point under Slow burn, keyed like _savedPoints. Not saved.
    std::unordered_set<RE::FormID> _paceCarry;
    // Skyrim.esm location keywords that make a calm place, looked up once at data load; null when missing.
    std::array<RE::BGSKeyword*, romantasy::pacing::kCalmPlaceKeywords.size()> _calmPlaceKeywords{};
    bool _loadDamaged = false;
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
    struct PendingLevelChange
    {
        nlohmann::json payload;
        bool whisper = true;  // false when a developer tool made the change
    };
    std::deque<PendingLevelChange> _pendingLevelChanges;
    // Kept in step with _pendingLevelChanges so the combat sink can skip the lock.
    std::atomic_bool _hasPendingLevelChanges = false;
    std::vector<RomanceFollower> _followers;
    // Guards _followers/_savedPoints/_lastStatValues/_pendingLevelChanges. Public
    // entry points are reached from the Papyrus VM thread (PapyrusBridge natives)
    // and game event threads as well as the main thread; recursive because locked
    // entry points call back into each other (e.g. ModifyPoints -> BuildStateJson).
    // Game tasks are deferred with _lock.Defer and queued only after the outermost
    // Scope unlocks: SKSE runs queued tasks under its own queue lock.
    romantasy::OutboxLock _lock;
    // What conditions and point getters read without _lock. Rebuilt and stored just before
    // the outermost Scope releases when _viewDirty is set; never null.
    std::atomic<std::shared_ptr<const romantasy::PointsView>> _view{ std::make_shared<const romantasy::PointsView>() };
    // Set by every entry point that may change what the view holds. Touched only under _lock.
    bool _viewDirty = false;
    // Set for the length of a ledger developer tool (DebugAddPoints, DebugApplyStat) and DebugSetPoints: what they
    // change raises no whisper. Touched only under _lock.
    bool _quietWhispers = false;
    // Paces the loaded-actor walk on statistic events; known followers are refreshed on
    // every event. Reset by Load and Revert so the first event after either walks.
    romantasy::Throttle _actorWalk{ std::chrono::seconds(5) };
    // The player's parent cell at the last statistic-path walk (0 when unknown or no cell);
    // a different cell walks at once.
    RE::FormID _actorWalkCell = 0;
    // Set wherever DiscoverFollowers() has run, so an empty roster is scanned once.
    bool _discoveryDone = false;
    bool _statsSinkRegistered = false;
    bool _combatSinkRegistered = false;
    bool _menuSinkRegistered = false;
    bool _calmSinksRegistered = false;
};
