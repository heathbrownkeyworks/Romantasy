#include "romance/SavedRomancePoints.h"
#include "RomanceManager.h"

#include "conditions/RomanceConditionQuery.h"

#include <charconv>
#include <chrono>

#include "health/Health.h"
#include "ostim/OStimBridge.h"
#include "romance/Why.h"
#include "settings/Settings.h"
#include "skyrimnet/BondLine.h"
#include "skyrimnet/SkyrimNetBridge.h"
#include "ui/JournalJson.h"
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
    constexpr std::uint32_t kManualPreferencesRecord = RecordType('R', 'o', 'P', 'm');
    constexpr std::uint32_t kManualPreferencesVersion = 1;
    constexpr std::uint32_t kPlayerEnrollmentRecord = RecordType('R', 'o', 'P', 'e');
    constexpr std::uint32_t kPlayerEnrollmentVersion = 1;
    constexpr std::uint32_t kPlayerBondsRecord = RecordType('R', 'o', 'P', 'b');
    constexpr std::uint32_t kMoodsRecord = RecordType('R', 'o', 'M', 'd');
    constexpr std::uint32_t kPendingRecord = RecordType('R', 'o', 'P', 'n');
    constexpr std::uint32_t kJournalsRecord = RecordType('R', 'o', 'J', 'n');
    constexpr std::uint32_t kMomentsRecord = RecordType('R', 'o', 'M', 'o');
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

    class SkseReader final : public romantasy::save::Reader
    {
    public:
        explicit SkseReader(SKSE::SerializationInterface* serialization) :
            _serialization(serialization)
        {}

        std::uint32_t Read(void* destination, std::uint32_t bytes) override
        {
            return _serialization->ReadRecordData(destination, bytes);
        }

    private:
        SKSE::SerializationInterface* _serialization;
    };

    class SkseWriter final : public romantasy::save::Writer
    {
    public:
        explicit SkseWriter(SKSE::SerializationInterface* serialization) :
            _serialization(serialization)
        {}

        bool Write(const void* source, std::uint32_t bytes) override
        {
            return _serialization->WriteRecordData(source, bytes);
        }

    private:
        SKSE::SerializationInterface* _serialization;
    };

    // Same plugin (names compare without case, as the game does) and same local ID.
    bool SameNamedForm(const romantasy::save::FormIdentity& a, const romantasy::save::FormIdentity& b)
    {
        return a.localID == b.localID && _stricmp(a.plugin.c_str(), b.plugin.c_str()) == 0;
    }

    // Plugin filename plus local ID for a runtime FormID. A dynamic form, or one whose
    // load-order slot has no file, keeps only its raw FormID.
    romantasy::save::FormIdentity IdentityForFormID(RE::FormID formID)
    {
        romantasy::save::FormIdentity identity;
        if (formID == 0) {
            return identity;
        }
        identity.rawFormID = formID;
        const auto parts = romantasy::save::SplitFormID(formID);
        if (parts.dynamic) {
            return identity;
        }
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return identity;
        }
        const auto* file = parts.light ?
            dataHandler->LookupLoadedLightModByIndex(parts.index) :
            dataHandler->LookupLoadedModByIndex(static_cast<std::uint8_t>(parts.index));
        if (!file) {
            return identity;
        }
        identity.plugin = std::string(file->GetFilename());
        identity.localID = parts.localID;
        return identity;
    }

    std::optional<RE::FormID> ResolveIdentity(
        const romantasy::save::FormIdentity& identity,
        SKSE::SerializationInterface* serialization)
    {
        if (identity == romantasy::save::FormIdentity{}) {
            return std::nullopt;
        }
        if (identity.plugin.empty()) {
            RE::FormID resolvedFormID = 0;
            if (!serialization->ResolveFormID(identity.rawFormID, resolvedFormID)) {
                return std::nullopt;
            }
            return resolvedFormID;
        }
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return std::nullopt;
        }
        const auto* file = dataHandler->LookupModByName(identity.plugin);
        if (!file) {
            return std::nullopt;
        }
        // LookupFormID adds the local ID to the file's index bits unchecked; a local ID that
        // does not fit the plugin's current slot (light now, regular when saved) would land
        // on another plugin's form.
        if (identity.localID > (file->compileIndex == 0xFE ? 0xFFFu : 0xFFFFFFu)) {
            return std::nullopt;
        }
        const auto formID = dataHandler->LookupFormID(identity.localID, identity.plugin);
        if (formID == 0) {
            return std::nullopt;
        }
        return formID;
    }

    // In-game days passed; 0 when the calendar is not available.
    float GameDay()
    {
        const auto* calendar = RE::Calendar::GetSingleton();
        return calendar ? calendar->GetDaysPassed() : 0.0f;
    }

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

    // Developer tools raise no whispers: set for the scope, restored after (they may nest).
    class QuietWhispers
    {
    public:
        explicit QuietWhispers(bool& flag) : _flag(flag), _was(std::exchange(flag, true)) {}
        ~QuietWhispers() { _flag = _was; }
        QuietWhispers(const QuietWhispers&) = delete;
        QuietWhispers& operator=(const QuietWhispers&) = delete;

    private:
        bool& _flag;
        bool _was;
    };

    // Broadcasts a Romantasy event to any Papyrus script that called
    // RegisterForModEvent. strArg may be empty; sender is the follower's base
    // NPC form so listeners can identify which follower the event is for.
    void SendBarkEvent(
        const romantasy::OutboxLock& outbox, const char* eventName, const char* strArg, float numArg, RE::TESForm* sender)
    {
        // Dispatch game-to-Papyrus callbacks on the main thread because callers may
        // arrive from the UI, tracked-stat, or combat event paths. The task is still
        // queued with AddTask, but only once the manager lock has been released.
        outbox.Defer(
            [name = RE::BSFixedString(eventName), str = RE::BSFixedString(strArg), numArg, sender]() {
                auto* source = SKSE::GetModCallbackEventSource();
                if (!source) {
                    return;
                }
                SKSE::ModCallbackEvent modEvent{ name, str, numArg, sender };
                source->SendEvent(&modEvent);
            });
    }

    // The player character's display name for SkyrimNet's texts, read as the follower names are read
    // (TESObjectREFR::GetDisplayFullName); "" before the player exists. The text builders clean it with Safe.
    std::string PlayerName()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        const char* name = player ? player->GetDisplayFullName() : nullptr;
        return name ? std::string(name) : std::string{};
    }

    // Tells SkyrimNet what Romantasy just told the follower's scripts, as narration for the follower. The text is
    // built now by `build` (under the lock, from plain values and the player's name); the send runs on the game
    // thread once the lock is released. Without SkyrimNet nothing is built, read or queued.
    template <class Build>
    void SendSkyrimNetEvent(const romantasy::OutboxLock& outbox, Build build, RE::FormID follower)
    {
        if (!SkyrimNetBridge::GetSingleton().Found() || !Settings::GetSingleton().SkyrimNetBridge() || follower == 0) {
            return;
        }
        outbox.Defer([text = std::string(build(PlayerName())), follower]() { SkyrimNetBridge::GetSingleton().SendEvent(text, follower); });
    }

    // Whole days for an event text; anything not finite or below 0 reads 0, which the text says as less than a day.
    std::int32_t WholeDays(float days)
    {
        if (!std::isfinite(days) || days <= 0.0f) {
            return 0;
        }
        return static_cast<std::int32_t>(std::floor((std::min)(days, 9999.0f)));
    }
}

RomanceManager::RomanceManager() :
    _lock([](romantasy::OutboxLock::Task task) { SKSE::GetTaskInterface()->AddTask(std::move(task)); })
{
    // Set before the lock is shared. Runs under the mutex on whichever thread closes the
    // outermost Scope, so a reader never sees a view older than the last released change.
    _lock.SetBeforeRelease([this]() {
        if (_viewDirty) {
            _view.store(BuildPointsView());
            _viewDirty = false;
        }
    });
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
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
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

    if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
        for (std::size_t index = 0; index < _calmPlaceKeywords.size(); ++index) {
            const auto localID = romantasy::pacing::kCalmPlaceKeywords[index];
            _calmPlaceKeywords[index] = dataHandler->LookupForm<RE::BGSKeyword>(localID, kSkyrimPlugin);
            if (!_calmPlaceKeywords[index]) {
                logger::warn("Romantasy could not find calm-place keyword {:06X} in {}", localID, kSkyrimPlugin);
            }
        }
    }

    _followers = DiscoverFollowers();
    _discoveryDone = true;
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

    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    QuietWhispers quiet(_quietWhispers);
    EnsureFollowersDiscovered();
    RefreshLoadedFollowers();
    logger::debug("Romantasy debug adding {} points to active followers ({} registered)", points, _followers.size());
    bool changedAnyFollower = false;
    for (auto& follower : _followers) {
        // Developer tool: not gated by Slow burn.
        changedAnyFollower = AddPoints(follower, points, "Debug adjustment", true, true, false) || changedAnyFollower;
    }

    if (changedAnyFollower) {
        // A tier change made here may arrive now, if this is a calm moment.
        ReleaseTierChanges(false);
        SendRuntimeState("Debug adjustment applied.");
    }
}

void RomanceManager::DebugApplyStat(std::string_view statName, std::int32_t delta)
{
    if (delta <= 0) {
        return;
    }

    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    QuietWhispers quiet(_quietWhispers);
    EnsureFollowersDiscovered();
    const auto* rule = FindStatRule(statName);
    if (!rule) {
        logger::warn("Romantasy debug stat not recognized: {}", statName);
        return;
    }

    logger::debug("Romantasy debug applying stat {} x{}", statName, delta);
    // Developer tool: paced like the deed it stands for, but not gated by Slow burn.
    if (ApplyStatDelta(*rule, delta, true, true, false)) {
        ReleaseTierChanges(false);
    }
}

bool RomanceManager::DebugDeed(RE::Actor* followerActor, std::string_view statistic, std::int32_t count)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    if (count <= 0) {
        return false;
    }
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::info("Romantasy DebugDeed did nothing: the actor is not a bond");
        return false;
    }
    const auto* rule = FindStatRule(statistic);
    if (!rule) {
        // The known names once per session; after that only the name that was not known.
        static bool listed = false;
        if (!listed) {
            listed = true;
            std::string names;
            for (const auto& known : kStatRules) {
                names += names.empty() ? "" : ", ";
                names += romantasy::why::CleanText(LabelForRule(known));
            }
            logger::info(
                "Romantasy DebugDeed does not know the deed \"{}\". Known deeds: {}", romantasy::why::CleanText(statistic), names);
        } else {
            logger::info("Romantasy DebugDeed does not know the deed \"{}\"", romantasy::why::CleanText(statistic));
        }
        return false;
    }

    // The deed's preference as a real event would see it, then the statistic path for this bond only.
    RefreshFollowerPreferences(*follower, followerActor, rule);
    logger::info("Romantasy DebugDeed: {} x{} for {}", LabelForRule(*rule), count, follower->name);
    // As a player would get it: paced, and held by Slow burn's gates (DebugSetPoints is the way
    // around a gate). Only the following requirement is off.
    const auto key = SavedPointsKey(*follower);
    const auto beforePoints = follower->points;
    const bool changed = ApplyStatDelta(*rule, count, true, false, true, key);
    if (changed) {
        ReleaseTierChanges(false);
    }
    // True only when the total moved: a loss on a bond already at 0 changed nothing. Found again by
    // key, because sending the state to an open ledger may reorder the followers.
    const auto* after = FindFollowerForKey(key);
    return after && after->points != beforePoints;
}

bool RomanceManager::DebugSetPoints(RE::Actor* followerActor, std::int32_t points)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::info("Romantasy DebugSetPoints did nothing: the actor is not a bond");
        return false;
    }
    // Points never go below 0 (AddPoints clamps there too); both totals are 0 or more, so the difference fits.
    const auto target = (std::max)(0, points);
    const auto delta = target - (std::max)(0, follower->points);
    logger::info("Romantasy DebugSetPoints: {} from {} to {}", follower->name, follower->points, target);
    if (delta == 0) {
        return true;
    }
    // Through AddPoints so the journal, mood and tier announcements behave as usual. Not gated, and no whispers.
    QuietWhispers quiet(_quietWhispers);
    if (!AddPoints(*follower, delta, "Debug set points", true, false, false)) {
        return false;
    }
    ReleaseTierChanges(false);
    SendRuntimeState("Romance records updated.");
    return true;
}

bool RomanceManager::DebugSetDaysTogether(RE::Actor* followerActor, float days)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    if (!std::isfinite(days)) {
        return false;
    }
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::info("Romantasy DebugSetDaysTogether did nothing: the actor is not a bond");
        return false;
    }
    const auto clamped = std::clamp(days, 0.0f, 100000.0f);
    MoodFor(*follower).company.daysTogether = clamped;
    logger::info("Romantasy DebugSetDaysTogether: {} now has {:.1f} day(s) together", follower->name, clamped);
    SendRuntimeState("Romance records updated.");
    return true;
}

bool RomanceManager::DebugSetMood(RE::Actor* followerActor, std::int32_t score)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::info("Romantasy DebugSetMood did nothing: the actor is not a bond");
        return false;
    }
    const auto today = GameDay();
    if (!romantasy::mood::detail::Finite(today)) {
        logger::info("Romantasy DebugSetMood did nothing: the game day is not a number");
        return false;
    }
    const auto clamped = std::clamp(score, -100, 100);
    // Stamped today, so the drift toward Settled starts now.
    MoodFor(*follower).mood = { static_cast<std::int16_t>(clamped), today };
    logger::info("Romantasy DebugSetMood: {} mood is now {}", follower->name, clamped);
    SendRuntimeState("Romance records updated.");
    return true;
}

bool RomanceManager::DebugAnnounceNow()
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    const auto waiting = _waitingTierChanges.size();
    // A rest is a calm moment wherever the player is, so the place is not looked at (no location).
    // In combat nothing is announced, as with any calm moment.
    ReleaseTierChanges(true, std::optional<const RE::BGSLocation*>(nullptr));
    logger::info(
        "Romantasy DebugAnnounceNow: {} tier change(s) were waiting, {} still wait",
        waiting,
        _waitingTierChanges.size());
    return _waitingTierChanges.empty();
}

std::string RomanceManager::DebugDescribe(RE::Actor* followerActor)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::info("Romantasy DebugDescribe did nothing: the actor is not a bond");
        return {};
    }
    // One line: text from mods is cleaned by the why-line cleaner (line breaks and '|' become
    // spaces, invalid UTF-8 '?'). A name is cut at 80 bytes and each list at 300, so the whole
    // line stays under 1700 bytes.
    constexpr std::size_t kListBytes = 300;
    const auto list = [&](const std::vector<std::string>& labels) {
        std::string out;
        for (const auto& label : labels) {
            out += out.empty() ? "" : ", ";
            out += romantasy::why::CleanText(label);
            if (out.size() > kListBytes) {
                break;
            }
        }
        return out.empty() ? std::string("none") : romantasy::why::CleanText(out, kListBytes);
    };

    const auto today = GameDay();
    const auto mood = MoodOrSettled(*follower);
    const auto days = romantasy::pacing::detail::SafeDays(DaysTogether(*follower));
    std::string gate = "none";
    if (Settings::GetSingleton().Pace() == romantasy::pacing::Pace::Slow) {
        if (const auto next = romantasy::pacing::NextGate(follower->points, days)) {
            gate = std::format(
                "{} needs {} days",
                LevelNameForPoints(MinimumPointsForFactionRank(static_cast<std::int8_t>(next->level - 1))),
                static_cast<std::int32_t>(next->daysNeeded));
        }
    }
    std::string personality = "none";
    if (const auto profile = _fileProfiles.find(follower->baseFormID); profile != _fileProfiles.end()) {
        personality = profile->second.playerOwned ? "player" : "author file";
    } else if (!follower->statDirections.empty()) {
        personality = "legacy";
    }

    auto line = std::format(
        "describe: {} | {} {} | mood {} ({}) | {:.1f} days together | gate {} | personality {} | likes {} | dislikes {} | favorites {} | pet peeves {}",
        romantasy::why::CleanText(follower->name),
        follower->points,
        LevelNameForPoints(follower->points),
        romantasy::mood::Name(romantasy::mood::StateAt(mood, today)),
        romantasy::mood::Drifted(mood, today),
        days,
        gate,
        personality,
        list(follower->likes),
        list(follower->dislikes),
        list(follower->favorites),
        list(follower->petPeeves));
    // Read without creating an entry, so describing never adds an empty record.
    const romantasy::ostim::MomentState none;
    const auto* moments = FindMoments(*follower);
    if (!moments) {
        moments = std::addressof(none);
    }
    const auto welcome = [](std::int32_t level) {
        return level >= 1 && level <= 6 ? std::string(LevelNameForPoints(MinimumPointsForFactionRank(static_cast<std::int8_t>(level - 1))))
                                        : std::string("never");
    };
    const auto feeling = follower->moments.feeling == romantasy::ostim::Feeling::Treasured ? "treasured"
                       : follower->moments.feeling == romantasy::ostim::Feeling::Reserved  ? "reserved"
                                                                                           : "warm";
    line += std::format(
        " | moments {}, last day {}, welcome {}/{}/{}, {}",
        moments->count,
        moments->lastDay >= 0.0f ? std::to_string(romantasy::journal::DayNumber(moments->lastDay)) : std::string("never"),
        welcome(follower->moments.affection),
        welcome(follower->moments.closeness),
        welcome(follower->moments.intimacy),
        feeling);
    logger::info("{}", line);
    return line;
}

bool RomanceManager::ModifyPoints(RE::Actor* followerActor, std::int32_t pointsDelta, std::string_view reason, bool showLevelUp)
{
    if (pointsDelta == 0) {
        return true;
    }

    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::warn("Romantasy ModifyPoints failed; follower actor was not romanceable or was unavailable");
        return false;
    }

    const auto eventLabel = reason.empty() ? std::string("Authored event") : std::string(reason);
    const auto oldPoints = follower->points;
    const bool changed = AddPoints(*follower, pointsDelta, eventLabel, showLevelUp);
    if (Settings::GetSingleton().DetailedLog()) {
        // AddPoints refuses a non-zero change only when not following or at the gate.
        const auto why = romantasy::why::Reason({
            .points = pointsDelta,
            .following = changed || IsFollowerFollowing(*follower),
            .paced = pointsDelta,
            .refused = !changed,
            .applied = follower->points - oldPoints,
        });
        if (why) {
            LogWhyNothing(*follower, std::nullopt, 0, *why);
        } else {
            logger::debug("{}", romantasy::why::Script(
                follower->name, follower->points - oldPoints, oldPoints, follower->points, LevelNameForPoints(follower->points)));
        }
    }
    if (changed) {
        // A tier change made here may arrive now, if this is a calm moment.
        ReleaseTierChanges(false);
        SendRuntimeState("Romance records updated.");
        return true;
    }

    // A gain a Slow burn gate refuses in full returns false, as a change of nothing does.
    return false;
}

bool RomanceManager::ApplyPreference(RE::Actor* followerActor, std::string_view statName, std::int32_t delta, bool showLevelUp)
{
    if (delta <= 0) {
        return false;
    }

    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
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
    const bool feeling = direction != follower->statDirections.end() && direction->second != 0;
    const auto pointsDelta = feeling ? romantasy::PreferencePoints(delta, rule->points, direction->second) : 0;
    if (!feeling || pointsDelta == 0) {
        if (!feeling) {
            logger::debug("Romantasy ApplyPreference ignored {}; {} has no configured preference", statName, follower->name);
        }
        if (Settings::GetSingleton().DetailedLog()) {
            if (const auto why = romantasy::why::Reason({ .feeling = feeling, .points = pointsDelta })) {
                LogWhyNothing(*follower, std::nullopt, 0, *why);
            }
        }
        return false;
    }

    const auto oldPoints = follower->points;
    const bool changed = AddPoints(*follower, pointsDelta, LabelForRule(*rule), showLevelUp);
    if (Settings::GetSingleton().DetailedLog()) {
        // AddPoints refuses a non-zero change only when not following or at the gate.
        const auto why = romantasy::why::Reason({
            .points = pointsDelta,
            .following = changed || IsFollowerFollowing(*follower),
            .paced = pointsDelta,
            .refused = !changed,
            .applied = follower->points - oldPoints,
        });
        if (why) {
            LogWhyNothing(*follower, std::nullopt, 0, *why);
        } else {
            logger::debug("{}", romantasy::why::Script(
                follower->name, follower->points - oldPoints, oldPoints, follower->points, LevelNameForPoints(follower->points)));
        }
    }
    if (changed) {
        // A tier change made here may arrive now, if this is a calm moment.
        ReleaseTierChanges(false);
        SendRuntimeState("Romance records updated.");
        return true;
    }

    // A gain a Slow burn gate refuses in full returns false, as a change of nothing does.
    return false;
}

bool RomanceManager::ClearPreferences(RE::Actor* followerActor)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
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
    logger::debug("Romantasy cleared all preferences for {} ({:08X})", follower->name, followerActor->GetFormID());
    SendRuntimeState("Follower preferences cleared.");
    return true;
}

bool RomanceManager::SetPreference(RE::Actor* followerActor, std::string_view statName, std::int32_t direction)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
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
    logger::debug(
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
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
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
        return romantasy::PapyrusDirection(direction->second);
    }
    return 0;
}

bool RomanceManager::SetPreferencesManual(RE::Actor* followerActor, bool manual)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
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

    logger::debug(
        "Romantasy marked preferences for {} ({:08X}) as {}",
        follower->name,
        identityFormID,
        manual ? "player-managed" : "author-managed");
    SendRuntimeState(manual ? "Preferences marked as player-managed." : "Preferences released to mod authors.");
    return true;
}

bool RomanceManager::IsPreferencesManual(RE::Actor* followerActor)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
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
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
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

    logger::debug(
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
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
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
    logger::debug(
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
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
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
    // A bond entered from the ledger starts a clean journal: one held from an earlier bond with this
    // NPC (a file-defined bond whose profile was deleted) must not carry its old tiers into a bond at 0.
    _journals.erase(identityFormID);
    _journals.erase(baseNpc->GetFormID());
    _moments.erase(identityFormID);
    _moments.erase(baseNpc->GetFormID());
    if (const auto* follower = FindCachedFollower(followerActor)) {
        romantasy::journal::Begin(JournalFor(*follower), LevelNumberForPoints(follower->points), GameDay());
    } else {
        romantasy::journal::Begin(_journals[identityFormID], LevelNumberForPoints(_savedPoints[identityFormID]), GameDay());
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
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
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
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    auto* follower = FindFollower(followerActor);
    if (!follower || follower->origin != RomanceProfileOrigin::kPlayerCreated) {
        logger::warn("Romantasy point reset rejected; profile was not player-created");
        return false;
    }

    // A reset is a fresh start in the journal: a history row and a fall to the first tier, no wound in the
    // moments or the day totals, and the tiers can be reached again.
    if (follower->points > 0) {
        romantasy::journal::RecordReset(
            JournalFor(*follower), "Bond reset", follower->points, LevelNumberForPoints(follower->points), GameDay());
    }
    follower->points = 0;
    follower->basePoints = 0;
    follower->recent.clear();
    _savedPoints[SavedPointsKey(*follower)] = 0;
    _moods.erase(SavedPointsKey(*follower));
    _waitingTierChanges.erase(SavedPointsKey(*follower));
    _paceCarry.erase(SavedPointsKey(*follower));
    if (follower->referenceFormID != 0 && AllowsLegacyBase(*follower)) {
        // A change or carry still under the base belongs to this bond too.
        _waitingTierChanges.erase(follower->baseFormID);
        _paceCarry.erase(follower->baseFormID);
    }
    _hasWaitingTierChanges = !_waitingTierChanges.empty();
    RefreshFollowerPreferences(*follower, followerActor);
    MirrorRelationshipLevel(*follower);
    logger::info("Romantasy reset the player-created bond with {}", follower->name);
    SendRuntimeState("Player-created bond reset.");
    return true;
}

bool RomanceManager::RemovePlayerFollower(RE::Actor* followerActor)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
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
    _moods.erase(identityFormID);
    _moods.erase(baseFormID);
    _journals.erase(identityFormID);
    _journals.erase(baseFormID);
    _moments.erase(identityFormID);
    _moments.erase(baseFormID);
    _waitingTierChanges.erase(identityFormID);
    _waitingTierChanges.erase(baseFormID);
    _paceCarry.erase(identityFormID);
    _paceCarry.erase(baseFormID);
    _conversations.erase(identityFormID);
    _conversations.erase(baseFormID);
    _manualPreferenceActors.erase(identityFormID);
    _playerEnrolledActors.erase(identityFormID);
    std::erase_if(_followers, [&](const auto& entry) {
        if (entry.baseFormID != baseFormID) return false;
        _savedPoints.erase(SavedPointsKey(entry));
        _moods.erase(SavedPointsKey(entry));
        _journals.erase(SavedPointsKey(entry));
        _moments.erase(SavedPointsKey(entry));
        _waitingTierChanges.erase(SavedPointsKey(entry));
        _paceCarry.erase(SavedPointsKey(entry));
        _conversations.erase(SavedPointsKey(entry));
        _manualPreferenceActors.erase(SavedPointsKey(entry));
        _playerEnrolledActors.erase(SavedPointsKey(entry));
        if (auto* actor = ResolveFollowerActor(entry)) ClearLegacyPlayerFactions(actor);
        return true;
    });
    _hasWaitingTierChanges = !_waitingTierChanges.empty();

    logger::info("Romantasy removed player-created bond {} ({:08X})", followerName, identityFormID);
    SendRuntimeState("Player-created bond removed.");
    return true;
}

namespace
{
    std::optional<std::int32_t> QueryPointsIn(const romantasy::PointsView& view, RE::Actor* followerActor)
    {
        auto* baseNpc = followerActor ? followerActor->GetActorBase() : nullptr;
        if (!baseNpc || followerActor == RE::PlayerCharacter::GetSingleton()) {
            return std::nullopt;
        }
        const auto base = baseNpc->GetFormID();
        // Same test SavedOrInitialPoints makes when a profile exists: base-keyed saves
        // migrate only to unique NPCs.
        auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(base);
        const bool baseIsUnique = npc && npc->IsUnique();
        return view.Query(followerActor->GetFormID(), base, baseIsUnique);
    }
}

std::optional<std::int32_t> RomanceManager::QueryPoints(RE::Actor* followerActor) const
{
    // Reads only the published view: no manager lock.
    return QueryPointsIn(*_view.load(), followerActor);
}

std::int32_t RomanceManager::QueryMood(RE::Actor* followerActor) const
{
    // Reads only the published view, loaded once so points and mood come from the same one: no manager lock.
    const auto view = _view.load();
    const auto points = QueryPointsIn(*view, followerActor);
    if (!points) {
        return 0;
    }
    std::optional<romantasy::mood::State> state;
    if (const auto mood = view->FindMood(followerActor->GetFormID())) {
        state = romantasy::mood::StateAt(*mood, GameDay());
    }
    return static_cast<std::int32_t>(romantasy::conditions::EvaluateMood(points, state));
}

std::string RomanceManager::SkyrimNetBondLine(RE::Actor* followerActor) const
{
    romantasy::skyrimnet::DecoratorFacts facts;
    facts.found = SkyrimNetBridge::GetSingleton().Found();
    facts.bridge = Settings::GetSingleton().SkyrimNetBridge();
    if (!followerActor || !facts.found || !facts.bridge) {
        return {};
    }
    romantasy::OutboxLock::Scope lock(_lock);
    // The cached bond only, without refreshing anything: a read that discovers or renames would change state from
    // SkyrimNet's prompt build.
    if (const auto* follower = MatchCachedFollower(followerActor)) {
        facts.bonded = true;
        facts.enabled = IsBondSwitchedOn(*follower);
        facts.bond.level = LevelNumberForPoints(follower->points);
        facts.bond.mood = static_cast<std::int32_t>(romantasy::mood::StateAt(MoodOrSettled(*follower), GameDay()));
        facts.bond.daysTogether = DaysTogether(*follower);
    }
    // Read here, on the VM thread, as this native already reads the actor SkyrimNet passed; BondLine cleans it.
    const std::string player = PlayerName();
    facts.bond.playerName = player;
    return romantasy::skyrimnet::DecoratorLine(facts);
}

bool RomanceManager::ConversationEligible(RE::Actor* followerActor, bool touched) const
{
    if (!followerActor || !SkyrimNetBridge::GetSingleton().Found() || !Settings::GetSingleton().SkyrimNetBridge() ||
        !Settings::GetSingleton().SkyrimNetConversation()) {
        return false;
    }
    romantasy::OutboxLock::Scope lock(_lock);
    // The cached bond only, without refreshing anything: SkyrimNet asks this while it chooses an action.
    const auto* follower = MatchCachedFollower(followerActor);
    if (!follower || !IsBondSwitchedOn(*follower)) {
        return false;
    }
    const auto state = _conversations.find(SavedPointsKey(*follower));
    return romantasy::skyrimnet::CanNudge(
        state != _conversations.end() ? state->second : romantasy::skyrimnet::ConversationState{}, touched, GameDay());
}

bool RomanceManager::ConversationNudge(RE::Actor* followerActor, bool touched)
{
    // SkyrimNet may run the action without a fresh eligibility answer: everything is checked again. Without SkyrimNet
    // or with a switch off nothing is touched, not even the lock.
    if (!followerActor || !SkyrimNetBridge::GetSingleton().Found() || !Settings::GetSingleton().SkyrimNetBridge() ||
        !Settings::GetSingleton().SkyrimNetConversation()) {
        return false;
    }
    // One scope for the check, the change and the note, so two calls in a row cannot both pass a limit.
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    auto* follower = FindCachedFollower(followerActor);
    if (!follower || !IsBondSwitchedOn(*follower)) {
        logger::debug("Romantasy conversation change ignored: the actor is not a switched-on bond");
        return false;
    }
    const auto key = SavedPointsKey(*follower);
    const auto today = GameDay();
    const auto state = _conversations.find(key);
    if (!romantasy::skyrimnet::CanNudge(
            state != _conversations.end() ? state->second : romantasy::skyrimnet::ConversationState{}, touched, today)) {
        logger::debug("Romantasy conversation change for {} ignored: the hour or day limit is reached", follower->name);
        return false;
    }
    // Not paced: Pace never multiplies a conversation change. Gated: Slow burn's gate still holds the gain.
    const std::int32_t delta = touched ? romantasy::skyrimnet::kConversationPoints : -romantasy::skyrimnet::kConversationPoints;
    const std::string_view label = touched ? "A kind word" : "A harsh word";
    const auto oldPoints = follower->points;
    const bool applied = AddPoints(*follower, delta, label, true, false, true);
    // Noted for every attempt that passed the limits, applied or held at the gate: a loop cannot spin.
    romantasy::skyrimnet::Noted(_conversations[key], touched, today);
    if (!applied) {
        logger::debug("Romantasy conversation change for {} held at the days-together gate", follower->name);
        LogWhyNothing(*follower, std::nullopt, 0, romantasy::why::Nothing::Gated);
        return false;
    }
    logger::info("Romantasy {}: {} point(s) for {} from conversation", label, delta, follower->name);
    if (Settings::GetSingleton().DetailedLog()) {
        logger::debug("{}", romantasy::why::Script(
            follower->name, follower->points - oldPoints, oldPoints, follower->points, LevelNameForPoints(follower->points)));
    }
    ReleaseTierChanges(false);
    SendRuntimeState("Romance records updated.");
    return true;
}

void RomanceManager::OnDialogueClosed()
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    ObserveCompany();
    ReleaseTierChanges(false);
}

void RomanceManager::OnSceneEnded(const romantasy::ostim::SceneEnd& scene)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    // A lost scene (a load, or OStim dropped the thread) is never judged.
    if (scene.lost && Settings::GetSingleton().DetailedLog()) {
        logger::debug("{}", romantasy::why::SceneNothing("the scene was lost"));
    }
    if (!scene.lost) {
        constexpr RE::FormID kPlayer = 0x14;
        bool playerPresent = false;
        std::vector<RE::FormID> bonds;
        for (const auto id : scene.participants) {
            if (id == kPlayer) {
                playerPresent = true;
                continue;
            }
            // The cached bond only: a scene's end discovers nothing. A participant that is no actor still counts in
            // participants, so an unknown third makes it not just the two of you.
            const auto* follower = MatchCachedFollower(RE::TESForm::LookupByID<RE::Actor>(id));
            if (follower && IsBondSwitchedOn(*follower)) {
                const auto key = SavedPointsKey(*follower);
                if (std::ranges::find(bonds, key) == bonds.end()) {
                    bonds.push_back(key);
                }
            }
        }
        if (bonds.empty() && Settings::GetSingleton().DetailedLog()) {
            logger::debug("{}", romantasy::why::SceneNothing("no switched-on bond in it"));
        }
        const auto today = GameDay();
        for (const auto key : bonds) {
            // Found again by key: AwardMoment sends the state, which may reorder the followers.
            const auto found = std::ranges::find_if(_followers, [&](const RomanceFollower& f) { return SavedPointsKey(f) == key; });
            if (found == _followers.end()) {
                continue;
            }
            const auto* moments = FindMoments(*found);
            (void)AwardMoment(*found, {
                .enabled = Settings::GetSingleton().OStimMoments(),
                .stoppedForMismatch = scene.stoppedForMismatch,
                .participants = static_cast<std::uint32_t>(scene.participants.size()),
                .playerPresent = playerPresent,
                .bonded = static_cast<std::uint32_t>(bonds.size()),
                .kind = scene.kind,
                .force = scene.force,
                .seconds = scene.seconds,
                .level = LevelNumberForPoints(found->points),
                .welcome = found->moments,
                .lastPointsDay = moments ? moments->lastPointsDay : -1.0f,
                .today = today,
            });
        }
    }
    FlushPendingLevelChanges();
    ReleaseTierChanges(false);
}

std::int32_t RomanceManager::GetMomentCount(RE::Actor* followerActor)
{
    if (!followerActor) {
        return 0;
    }
    romantasy::OutboxLock::Scope lock(_lock);
    // The cached bond and its moments, found without creating anything.
    const auto* follower = MatchCachedFollower(followerActor);
    const auto* moments = follower ? FindMoments(*follower) : nullptr;
    if (!moments) {
        return 0;
    }
    return static_cast<std::int32_t>((std::min)(moments->count, static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)())));
}

float RomanceManager::GetDaysSinceMoment(RE::Actor* followerActor)
{
    if (!followerActor) {
        return -1.0f;
    }
    romantasy::OutboxLock::Scope lock(_lock);
    const auto* follower = MatchCachedFollower(followerActor);
    const auto* moments = follower ? FindMoments(*follower) : nullptr;
    if (!moments) {
        return -1.0f;
    }
    return romantasy::ostim::DaysSince(*moments, GameDay());
}

bool RomanceManager::DebugSharedMoment(RE::Actor* followerActor, std::string_view kind, float seconds, bool others, bool force)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    const auto parsed = romantasy::ostim::ParseKind(kind);
    if (!parsed) {
        logger::info(
            "Romantasy DebugSharedMoment does not know the kind \"{}\". Known kinds: affection, closeness, intimacy",
            romantasy::why::CleanText(kind));
        return false;
    }
    auto* follower = FindFollower(followerActor);
    if (!follower) {
        logger::info("Romantasy DebugSharedMoment did nothing: the actor is not a bond");
        return false;
    }
    if (!IsBondSwitchedOn(*follower)) {
        logger::info("Romantasy DebugSharedMoment did nothing: {}'s bond is switched off", follower->name);
        return false;
    }
    logger::info("Romantasy DebugSharedMoment: {} for {}", romantasy::ostim::Key(*parsed), follower->name);
    const auto* moments = FindMoments(*follower);
    return AwardMoment(*follower, {
        .enabled = Settings::GetSingleton().OStimMoments(),
        .participants = others ? 3u : 2u,
        .playerPresent = true,
        .bonded = 1,
        .kind = *parsed,
        .force = force,
        .seconds = seconds,
        .level = LevelNumberForPoints(follower->points),
        .welcome = follower->moments,
        .lastPointsDay = moments ? moments->lastPointsDay : -1.0f,
        .today = GameDay(),
    });
}

std::int32_t RomanceManager::TryShareMoment(RE::Actor* followerActor, std::string_view kind)
{
    using romantasy::ostim::StartCode;
    if (!followerActor || followerActor->IsPlayerRef()) {
        return static_cast<std::int32_t>(StartCode::NotABond);
    }
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    const RomanceFollower* follower = nullptr;
    const auto answer = CheckMomentStart(followerActor, kind, follower);
    const auto code = static_cast<std::int32_t>(answer.code);
    const std::string kindKey(romantasy::ostim::Key(answer.kind));
    if (answer.code == StartCode::TooSoon || answer.code == StartCode::Strained) {
        const bool tooSoon = answer.code == StartCode::TooSoon;
        logger::info("Romantasy TryShareMoment: {} is {} for a moment ({})", follower->name, tooSoon ? "not ready" : "strained",
            kindKey.empty() ? "none welcome" : kindKey);
        RaiseWhisper({ .kind = tooSoon ? romantasy::ui::whispers::Kind::MomentNotYet : romantasy::ui::whispers::Kind::MomentNotInMood,
            .follower = SavedPointsKey(*follower),
            .name = follower->name });
        if (auto* senderForm = RE::TESForm::LookupByID(follower->baseFormID)) {
            SendBarkEvent(_lock, "Romantasy_OnMomentRefused", kindKey.c_str(), static_cast<float>(code), senderForm);
        }
        return code;
    }
    if (answer.code != StartCode::Accepted) {
        return code;
    }
    // One start at a time: a second call that raced past the check above is told "not now".
    if (!OStimBridge::GetSingleton().ClaimStart()) {
        return static_cast<std::int32_t>(StartCode::NotNow);
    }
    // Names this claim, so the deferred begin cannot take up a newer one.
    const std::uint32_t stamp = OStimBridge::GetSingleton().ClaimedStamp();
    logger::info("Romantasy TryShareMoment: starting a moment ({}) with {}", kindKey, follower->name);
    _lock.Defer([handle = followerActor->GetHandle(), startKind = answer.kind, stamp]() {
        OStimBridge::GetSingleton().BeginMoment(handle, startKind, stamp);
    });
    return code;
}

std::int32_t RomanceManager::CanShareMoment(RE::Actor* followerActor, std::string_view kind) const
{
    if (!followerActor || followerActor->IsPlayerRef()) {
        return static_cast<std::int32_t>(romantasy::ostim::StartCode::NotABond);
    }
    romantasy::OutboxLock::Scope lock(_lock);
    const RomanceFollower* follower = nullptr;
    return static_cast<std::int32_t>(CheckMomentStart(followerActor, kind, follower).code);
}

void RomanceManager::OnMomentCouldNotStart(RE::FormID followerID, romantasy::ostim::Kind kind)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    const auto* follower = MatchCachedFollower(followerID ? RE::TESForm::LookupByID<RE::Actor>(followerID) : nullptr);
    if (!follower) {
        logger::info("Romantasy: a moment ({}) could not start, and its follower is not a bond here", romantasy::ostim::Key(kind));
        return;
    }
    logger::info("Romantasy: a moment ({}) with {} could not start", romantasy::ostim::Key(kind), follower->name);
    if (auto* senderForm = RE::TESForm::LookupByID(follower->baseFormID)) {
        SendBarkEvent(_lock, "Romantasy_OnMomentRefused", std::string(romantasy::ostim::Key(kind)).c_str(),
            static_cast<float>(romantasy::ostim::StartCode::CouldNotStart), senderForm);
    }
}

void RomanceManager::OnGameLoaded()
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    // Here, not in the cosave callback: the calendar is restored by now. A damaged clock
    // value would otherwise credit the whole calendar as days together.
    const auto today = GameDay();
    // _viewDirty is already set above, so a re-seeded stamp reaches the published view.
    for (auto& [identityFormID, bond] : _moods) {
        const auto fixed = romantasy::mood::Reseed(bond.mood, bond.company, today);
        if (fixed.changed) {
            bond.mood = fixed.mood;
            bond.company = fixed.company;
        }
    }
    // A waiting change stamped with a damaged or future day starts waiting today.
    if (romantasy::pacing::detail::Finite(today)) {
        for (auto& [identityFormID, change] : _waitingTierChanges) {
            if (!romantasy::pacing::detail::Finite(change.since) || change.since < 0.0f || change.since > today) {
                change.since = today;
            }
        }
    }
    ObserveCompany();
}

std::int32_t RomanceManager::GetPoints(RE::Actor* followerActor)
{
    std::int32_t points = 0;
    (void)TryGetPoints(followerActor, points);
    return points;
}

bool RomanceManager::TryGetPoints(RE::Actor* followerActor, std::int32_t& points)
{
    if (followerActor) {
        if (const auto known = _view.load()->Find(followerActor->GetFormID())) {
            points = *known;
            return true;
        }
    }

    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    if (const auto* follower = FindFollower(followerActor)) {
        points = follower->points;
        return true;
    }

    points = 0;
    return false;
}

std::int32_t RomanceManager::GetLevel(RE::Actor* followerActor)
{
    if (followerActor) {
        if (const auto known = _view.load()->Find(followerActor->GetFormID())) {
            return LevelNumberForPoints(*known);
        }
    }

    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    if (const auto* follower = FindFollower(followerActor)) {
        return LevelNumberForPoints(follower->points);
    }

    return 0;
}

std::string RomanceManager::GetLevelName(RE::Actor* followerActor)
{
    if (followerActor) {
        if (const auto known = _view.load()->Find(followerActor->GetFormID())) {
            return std::string(LevelNameForPoints(*known));
        }
    }

    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    if (const auto* follower = FindFollower(followerActor)) {
        return std::string(LevelNameForPoints(follower->points));
    }

    return {};
}

nlohmann::json RomanceManager::BuildStateJson(std::string_view message)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    RefreshLoadedFollowers();
    // Identities first, so a reference assigned here is looked at under its own key.
    for (auto& follower : _followers) {
        if (auto* actor = ResolveFollowerActor(follower)) {
            RefreshFollowerIdentity(follower, actor);
        }
    }
    ObserveCompany();
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    float currentDay = 0.0f;
    if (const auto* calendar = RE::Calendar::GetSingleton()) {
        currentDay = calendar->GetDaysPassed();
    }
    std::ranges::sort(_followers, [](const auto& lhs, const auto& rhs) {
        return lhs.name < rhs.name;
    });

    const auto pace = Settings::GetSingleton().Pace();
    nlohmann::json romances = nlohmann::json::array();
    for (const auto& follower : _followers) {
        nlohmann::json recent = nlohmann::json::array();
        for (const auto& event : follower.recent) {
            nlohmann::json entry = {
                { "label", event.label },
                { "points", event.points },
            };
            // The game day of the change: the page orders the Recent roster by it.
            if (std::isfinite(event.gameDay) && event.gameDay >= 0.0f) {
                entry["day"] = event.gameDay;
            }
            if (event.gameDay > 0.0f && currentDay >= event.gameDay) {
                const auto daysAgo = static_cast<std::int32_t>(currentDay - event.gameDay);
                // Words for the reader; the page's roster parses them only when "day" is missing.
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
        auto unlocks = nlohmann::json::array();
        if (profile != _fileProfiles.end()) {
            for (std::size_t level = 2; level <= profile->second.unlocks.size(); ++level) {
                const auto& items = profile->second.unlocks[level - 1];
                if (!items.empty()) {
                    unlocks.push_back({ { "level", level }, { "items", items } });
                }
            }
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
            { "profileFile", profileFile },
            { "points", follower.points },
            { "levelName", std::string(LevelNameForPoints(follower.points)) },
            { "nextGoal", NextLevelNameForPoints(follower.points) },
            { "isFollowing", IsFollowerFollowing(follower) },
            { "mood", std::string(romantasy::mood::Key(romantasy::mood::StateAt(MoodOrSettled(follower), currentDay))) },
            { "role", BuildRoleLabel(follower) },
            { "likes", follower.likes },
            { "dislikes", follower.dislikes },
            { "favorites", follower.favorites },
            { "petPeeves", follower.petPeeves },
            { "unlocks", unlocks },
            { "recent", recent },
        });
        // Slow burn: the next tier that still needs days together.
        if (pace == romantasy::pacing::Pace::Slow) {
            const auto daysTogether = DaysTogether(follower);
            if (const auto gate = romantasy::pacing::NextGate(follower.points, daysTogether)) {
                romances.back()["gate"] = {
                    { "level", gate->level },
                    { "name", std::string(LevelNameForPoints(MinimumPointsForFactionRank(static_cast<std::int8_t>(gate->level - 1)))) },
                    { "daysNeeded", static_cast<std::int32_t>(gate->daysNeeded) },
                    { "daysTogether", romantasy::pacing::detail::SafeDays(daysTogether) },
                };
            }
        }
    }

    return {
        { "plugin", Plugin::NAME },
        { "message", message },
        { "timestamp", seconds },
        { "openWithFavorites", Settings::GetSingleton().OpenWithFavorites() },
        { "showGainModals", Settings::GetSingleton().ShowGainModals() },
        { "showLossModals", Settings::GetSingleton().ShowLossModals() },
        { "showAwayFollowers", Settings::GetSingleton().ShowAwayFollowers() },
        { "detailedLog", Settings::GetSingleton().DetailedLog() },
        { "pace", romantasy::pacing::Key(Settings::GetSingleton().Pace()) },
        { "tierMoment", romantasy::pacing::Key(Settings::GetSingleton().TierMoment()) },
        { "whispers", Settings::GetSingleton().Whispers() },
        { "whisperCorner", romantasy::ui::whispers::Key(Settings::GetSingleton().WhisperCorner()) },
        { "rosterOrder", std::string(Settings::GetSingleton().RosterOrder()) },
        { "ledgerSize", std::string(Settings::GetSingleton().LedgerSize()) },
        { "highContrast", Settings::GetSingleton().HighContrast() },
        { "reduceMotion", Settings::GetSingleton().ReduceMotion() },
        { "skyrimNet", { { "found", SkyrimNetBridge::GetSingleton().Found() }, { "bridge", Settings::GetSingleton().SkyrimNetBridge() },
            { "conversation", Settings::GetSingleton().SkyrimNetConversation() } } },
        { "ostim", { { "found", OStimBridge::GetSingleton().Found() }, { "moments", Settings::GetSingleton().OStimMoments() },
            { "hold", Settings::GetSingleton().OStimHold() } } },
        { "romances", romances },
        { "enrollmentCandidates", BuildEnrollmentCandidatesJson() },
        { "preferenceOptions", BuildPreferenceOptionsJson() },
    };
}

nlohmann::json RomanceManager::BuildJournalJson(std::string_view identity)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    // The identity text resolves as a bond request's does: a reference first, then the base NPC's unique actor.
    RE::Actor* actor = nullptr;
    std::uint32_t formID = 0;
    const auto* first = identity.data();
    const auto* last = identity.data() + identity.size();
    if (const auto [end, error] = std::from_chars(first, last, formID, 16);
        error == std::errc{} && end == last && formID != 0) {
        actor = RE::TESForm::LookupByID<RE::Actor>(formID);
        if (!actor) {
            if (auto* baseNpc = RE::TESForm::LookupByID<RE::TESNPC>(formID)) {
                actor = baseNpc->GetUniqueActor();
            }
        }
    }
    const auto* follower = FindFollower(actor);
    if (!follower) {
        return romantasy::ui::JournalJson(identity, nullptr, 0.0f);
    }
    // Read without creating an entry; one still under the base is found when JournalFor would adopt it.
    const romantasy::journal::Journal* journal = nullptr;
    if (const auto found = _journals.find(SavedPointsKey(*follower)); found != _journals.end()) {
        journal = std::addressof(found->second);
    } else if (follower->referenceFormID != 0) {
        auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(follower->baseFormID);
        const bool allowLegacyBase = !_fileProfiles.contains(follower->baseFormID) || (npc && npc->IsUnique());
        if (const auto legacy = _journals.find(follower->baseFormID); allowLegacyBase && legacy != _journals.end()) {
            journal = std::addressof(legacy->second);
        }
    }
    // A bond with no journal yet answers found, with empty lists.
    // The first shared moment's day, found the same way; -1 when there is none.
    const auto* moments = FindMoments(*follower);
    const float firstMomentDay = moments ? moments->firstDay : -1.0f;
    const romantasy::journal::Journal empty;
    return romantasy::ui::JournalJson(
        identity, journal ? journal : std::addressof(empty), DaysTogether(*follower), firstMomentDay);
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
    // The lines of every tier gained in this change, lowest first; none on a loss.
    auto unlocks = nlohmann::json::array();
    if (const auto profile = _fileProfiles.find(follower.baseFormID);
        profile != _fileProfiles.end() && newLevel > oldLevel) {
        const auto& tiers = profile->second.unlocks;
        for (auto level = (std::max)(oldLevel + 1, 2); level <= (std::min)(newLevel, static_cast<std::int32_t>(tiers.size())); ++level) {
            for (const auto& line : tiers[static_cast<std::size_t>(level - 1)]) {
                unlocks.push_back(line);
            }
        }
    }

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
        { "unlocks", unlocks },
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
    _authorProfiles.clear();
    _playerLibrary.clear();
    _libraryOrphans.clear();
    _profileProblems.clear();
    auto result = romantasy::LoadProfiles("Data/SKSE/Plugins/Romantasy/Profiles");
    auto playerResult = romantasy::LoadProfiles(kPlayerProfilesDirectory, true);
    std::ranges::move(playerResult.profiles, std::back_inserter(result.profiles));
    std::ranges::move(playerResult.errors, std::back_inserter(result.errors));
    for (const auto& error : result.errors) {
        logger::error("Romantasy profile: {}", error);
        std::ranges::move(romantasy::health::ProfileProblemsFromLoaderError(error), std::back_inserter(_profileProblems));
    }
    if (!RE::TESDataHandler::GetSingleton()) {
        logger::error("Romantasy cannot resolve file profiles; data handler unavailable");
        return;
    }
    for (auto& profile : result.profiles) {
        std::string failure;
        bool pluginUnavailable = false;
        bool pluginNotLoaded = false;
        auto* npc = ResolveProfileNpc(
            profile.plugin, profile.localID, profile.source, &failure, &pluginUnavailable, &pluginNotLoaded);
        if (!npc) {
            logger::error("Romantasy profile {}: {}", profile.source, failure);
            // A mod that is simply not loaded is not a problem with the file (its bond is kept below).
            if (!pluginNotLoaded) {
                _profileProblems.push_back(std::filesystem::path(profile.source).filename().string() + ": " + failure);
            }
            if (profile.playerOwned && pluginUnavailable) {
                // Kept as a save bond, so a save made while the plugin is missing still carries it.
                romantasy::save::PlayerBond bond;
                bond.base = { .plugin = profile.plugin, .localID = profile.localID };
                bond.enabled = profile.enabled;
                for (const auto& [statName, direction] : profile.preferences) {
                    bond.preferences.emplace_back(statName, static_cast<std::int8_t>(direction));
                }
                std::ranges::sort(bond.preferences);
                if (std::ranges::none_of(_libraryOrphans, [&](const auto& kept) { return SameNamedForm(kept.base, bond.base); })) {
                    _libraryOrphans.push_back(std::move(bond));
                }
            }
            continue;
        }
        if (profile.playerOwned && (_authorProfiles.contains(npc->GetFormID()) ||
                _playerLibrary.contains(npc->GetFormID()) ||
                (_romanceLevelFaction && npc->IsInFaction(_romanceLevelFaction)))) {
            logger::warn("Romantasy ignored player profile {}; a mod owns this NPC's romance", profile.source);
            continue;
        }
        logger::info("Romantasy loaded profile {} for {:08X}", profile.source, npc->GetFormID());
        (profile.playerOwned ? _playerLibrary : _authorProfiles).emplace(npc->GetFormID(), std::move(profile));
    }
    UseLibraryBonds();
    logger::info(
        "Romantasy loaded {} file-defined NPC profile(s) and kept {} player profile(s) whose plugin is not loaded",
        _fileProfiles.size(),
        _libraryOrphans.size());
}

RE::TESNPC* RomanceManager::ResolveProfileNpc(
    std::string_view plugin, std::uint32_t localID, std::string_view source, std::string* failure,
    bool* pluginUnavailable, bool* pluginNotLoaded) const
{
    // With `failure`, the reason is returned to the caller instead of logged. `pluginUnavailable` also covers a light
    // plugin's local ID above 0xFFF; `pluginNotLoaded` only a plugin that is missing or present but not active.
    if (pluginUnavailable) {
        *pluginUnavailable = false;
    }
    if (pluginNotLoaded) {
        *pluginNotLoaded = false;
    }
    const auto fail = [&](std::string_view reason) -> RE::TESNPC* {
        if (failure) {
            *failure = reason;
        } else {
            logger::error("Romantasy profile {}: {}", source, reason);
        }
        return nullptr;
    };
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    const auto* file = dataHandler ? dataHandler->LookupModByName(plugin) : nullptr;
    // A plugin in Data but not active has compile index 0xFF; LookupFormID refuses it, as
    // ResolveIdentity relies on. LookupForm masks ESL IDs; reject oversized local IDs before resolution.
    if (!file || file->compileIndex == 0xFF || (file->IsLight() && localID > 0xFFF)) {
        if (pluginUnavailable) {
            *pluginUnavailable = true;
        }
        if (pluginNotLoaded) {
            *pluginNotLoaded = !file || file->compileIndex == 0xFF;
        }
        return fail("plugin unavailable or local ID outside its range");
    }
    auto* npc = dataHandler->LookupForm<RE::TESNPC>(localID, plugin);
    if (!npc || npc->IsDeleted() || npc->GetFormID() == 7 || (npc->GetRace() && npc->GetRace()->IsChildRace())) {
        return fail("NPC is unavailable, deleted, a child, or the player");
    }
    return npc;
}

void RomanceManager::UseLibraryBonds()
{
    // The state 3.0.x has: author profiles plus every player file on disk, disabled ones included.
    _fileProfiles = _authorProfiles;
    for (const auto& [formID, profile] : _playerLibrary) {
        if (!_authorProfiles.contains(formID)) {
            _fileProfiles.emplace(formID, profile);
        }
    }
    // Files whose plugin is not loaded go to the save as orphans and resolve when it returns.
    _orphanedBonds = _libraryOrphans;
}

void RomanceManager::ApplyPlayerBonds(
    const romantasy::save::ReadResult<romantasy::save::PlayerBond>& bonds,
    SKSE::SerializationInterface* serialization)
{
    // The save is the truth for its player bonds; files on disk for other NPCs are ignored.
    _fileProfiles = _authorProfiles;
    std::size_t applied = 0;
    for (const auto& bond : bonds.entries) {
        if (bond.base.plugin.empty()) {
            // Never written by Romantasy (dynamic bases cannot be bonded); a raw FormID is not safe to keep.
            logger::warn("Romantasy dropped a saved player bond with no plugin (FormID {:08X})", bond.base.rawFormID);
            continue;
        }
        const auto source = std::format("saved bond {:06X} in '{}'", bond.base.localID, bond.base.plugin);
        const auto resolved = ResolveIdentity(bond.base, serialization);
        std::string failure = "plugin not loaded or local ID outside its range";
        auto* npc = resolved ? ResolveProfileNpc(bond.base.plugin, bond.base.localID, source, &failure) : nullptr;
        if (npc && npc->GetFormID() != *resolved) {
            npc = nullptr;
            failure = "NPC did not resolve to the saved identity";
        }
        if (!npc) {
            logger::warn("Romantasy is keeping {} unused; {}", source, failure);
            _orphanedBonds.push_back(bond);
            continue;
        }
        const auto formID = npc->GetFormID();
        if (_authorProfiles.contains(formID) || (_romanceLevelFaction && npc->IsInFaction(_romanceLevelFaction))) {
            logger::warn("Romantasy is keeping {} unused; a mod owns this NPC's romance", source);
            _orphanedBonds.push_back(bond);
            continue;
        }
        romantasy::NpcProfile profile;
        profile.plugin = bond.base.plugin;
        profile.localID = bond.base.localID;
        profile.playerOwned = true;
        profile.enabled = bond.enabled;
        // The ledger shows the source as the profile file: the library's file, or nothing.
        const auto library = _playerLibrary.find(formID);
        profile.source = library != _playerLibrary.end() ? library->second.source : std::string{};
        for (const auto& [statName, direction] : bond.preferences) {
            profile.preferences.insert_or_assign(statName, direction);
        }
        if (const auto demoted = romantasy::EnforceStrongCap(profile.preferences); demoted > 0) {
            logger::warn("Romantasy demoted {} favorite or pet peeve weight(s) beyond the limit of {} in {}", demoted, romantasy::kMaxStrongPreferences, source);
        }
        if (!_fileProfiles.emplace(formID, std::move(profile)).second) {
            logger::warn("Romantasy ignored a duplicate {}", source);
            continue;
        }
        ++applied;
    }
    if (!bonds.complete) {
        // A damaged record cannot say which bonds were cut off; fall back to the files on disk
        // for NPCs it does not mention, as a save without the record would.
        for (const auto& [formID, profile] : _playerLibrary) {
            if (!_authorProfiles.contains(formID) && _fileProfiles.try_emplace(formID, profile).second) {
                logger::warn("Romantasy restored the player bond for {:08X} from {}; the save's bond record is damaged", formID, profile.source);
            }
        }
        for (const auto& orphan : _libraryOrphans) {
            const auto mentioned = std::ranges::any_of(bonds.entries, [&](const auto& bond) {
                return SameNamedForm(bond.base, orphan.base);
            });
            if (!mentioned) {
                logger::warn(
                    "Romantasy restored the player bond for {:06X} in '{}' from disk; the save's bond record is damaged",
                    orphan.base.localID,
                    orphan.base.plugin);
                _orphanedBonds.push_back(orphan);
            }
        }
    }
    logger::info(
        "Romantasy applied {} saved player bond(s) and kept {} whose NPC is unavailable or mod-owned",
        applied,
        _orphanedBonds.size());
}

void RomanceManager::ApplyFilePreferences(RomanceFollower& follower, const romantasy::NpcProfile& profile) const
{
    follower.statDirections.clear();
    follower.likes.clear();
    follower.dislikes.clear();
    follower.favorites.clear();
    follower.petPeeves.clear();
    follower.moments = profile.moments;
    for (const auto& rule : kStatRules) {
        const auto preference = profile.preferences.find(std::string(rule.editorID));
        if (preference == profile.preferences.end()) {
            continue;
        }
        // String views refer to the static statistic table, never temporary TOML strings.
        follower.statDirections.emplace(rule.editorID, preference->second);
        const auto label = LabelForRule(rule);
        (preference->second < 0 ? follower.dislikes : follower.likes).push_back(label);
        if (preference->second == 2) {
            follower.favorites.push_back(label);
        } else if (preference->second == -2) {
            follower.petPeeves.push_back(label);
        }
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
        if (direction < -2 || direction > 2) {
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
    if (const auto reason = romantasy::CheckPreferenceWeights(resolved)) {
        logger::warn("Romantasy preference profile rejected: {}", *reason);
        return false;
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
    // The save is the truth: the bond changes here and reaches the cosave on the next save.
    _fileProfiles.insert_or_assign(npc->GetFormID(), profile);
    if (enabled) {
        // The file on disk is only a copy that carries the personality to new characters.
        // Removal never touches it, so other saves keep their bond.
        auto copy = profile;
        const auto library = _playerLibrary.find(npc->GetFormID());
        copy.source = library != _playerLibrary.end() ? library->second.source : std::string{};
        if (library != _playerLibrary.end()) {
            // The save record does not carry the starting level; keep the file's own.
            copy.startingLevel = library->second.startingLevel;
        }
        try {
            auto saved = romantasy::SavePlayerProfile(kPlayerProfilesDirectory, copy);
            logger::info("Romantasy saved player profile {} (enabled={})", saved.source, saved.enabled);
            _playerLibrary.insert_or_assign(npc->GetFormID(), std::move(saved));
        } catch (const std::exception& e) {
            logger::warn("Romantasy could not write the player profile copy; the bond is kept in the save: {}", e.what());
        }
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
    if (const auto library = _playerLibrary.find(npc->GetFormID()); library != _playerLibrary.end()) {
        // A personality file already exists: adopt it into this save as it is (3.0.x used the
        // file and only cleared the factions). Nothing is written to disk.
        _fileProfiles.insert_or_assign(npc->GetFormID(), library->second);
        ClearLegacyPlayerFactions(actor);
        _playerEnrolledActors.erase(ActorIdentity(actor));
        _playerEnrolledActors.erase(npc->GetFormID());
        logger::info("Romantasy adopted player profile {} for legacy bond {:08X}", library->second.source, ActorIdentity(actor));
        return;
    }
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
    // _followers is not const here, so the matcher's answer may be written through.
    auto* matched = const_cast<RomanceFollower*>(MatchCachedFollower(followerActor));
    if (matched && matched->referenceFormID == 0) {
        // Matched by its base: learn the reference.
        RefreshFollowerIdentity(*matched, followerActor);
    }
    return matched;
}

const RomanceFollower* RomanceManager::MatchCachedFollower(RE::Actor* followerActor) const
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
    return follower != _followers.end() ? std::addressof(*follower) : nullptr;
}

bool RomanceManager::IsBondSwitchedOn(const RomanceFollower& follower) const
{
    const auto profile = _fileProfiles.find(follower.baseFormID);
    return profile == _fileProfiles.end() || profile->second.enabled;
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

    // No file (or its file was removed): moments keep the defaults.
    follower.moments = {};
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
        std::erase(follower.favorites, label);
        std::erase(follower.petPeeves, label);

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
        follower.favorites.clear();
        follower.petPeeves.clear();
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
        logger::debug(
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

bool RomanceManager::SceneHolds() const
{
    return OStimBridge::GetSingleton().InScene() && Settings::GetSingleton().OStimHold();
}

romantasy::ostim::StartAnswer RomanceManager::CheckMomentStart(
    RE::Actor* followerActor, std::string_view kind, const RomanceFollower*& follower) const
{
    const auto& bridge = OStimBridge::GetSingleton();
    romantasy::ostim::StartFacts facts;
    facts.canStart = bridge.CanStart();
    facts.asked = romantasy::ostim::ParseKind(kind);
    facts.kindKnown = kind.empty() || facts.asked.has_value();
    facts.inCombat = IsPlayerInCombat();
    facts.inScene = bridge.InScene();
    facts.pending = bridge.StartPending();
    // The cached bond only: asking discovers nothing.
    follower = MatchCachedFollower(followerActor);
    if (follower && !IsBondSwitchedOn(*follower)) {
        follower = nullptr;
    }
    if (follower) {
        facts.bonded = true;
        facts.level = LevelNumberForPoints(follower->points);
        facts.mood = static_cast<std::int32_t>(romantasy::mood::StateAt(MoodOrSettled(*follower), GameDay()));
        facts.welcome = follower->moments;
    }
    return romantasy::ostim::CheckStart(facts);
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

bool RomanceManager::AddPoints(
    RomanceFollower& follower, std::int32_t pointsDelta, std::string_view reason, bool showLevelUp, bool requireFollowing,
    bool gated)
{
    if (pointsDelta == 0) {
        return false;
    }

    if (requireFollowing && !IsFollowerFollowing(follower)) {
        logger::debug(
            "Romantasy skipped {} point adjustment for {}; follower is not actively following",
            reason,
            follower.name
        );
        return false;
    }

    const auto oldPoints = follower.points;
    const auto oldLevel = LevelNumberForPoints(follower.points);
    const auto gate = romantasy::pacing::GateGain(
        oldPoints, pointsDelta, DaysTogether(follower), Settings::GetSingleton().Pace(), gated);
    if (gate.refused) {
        logger::debug(
            "Romantasy held {} point(s) for {} at the days-together gate; {:.1f} day(s) together",
            pointsDelta,
            follower.name,
            DaysTogether(follower));
        return false;
    }
    // What the change row and the popup show: the change before the clamp at 0, after the gate.
    const auto shownDelta = gate.applied;
    follower.points = (std::max)(0, gate.newPoints);
    _savedPoints[SavedPointsKey(follower)] = follower.points;
    PushRecentEvent(follower, std::string(reason), shownDelta);
    MirrorRelationshipLevel(follower);
    const auto today = GameDay();
    auto& bond = MoodFor(follower);
    const auto moodBefore = romantasy::mood::StateAt(bond.mood, today);
    // The requested change on purpose, not the points left after the clamp at 0: mood reacts to the deed.
    bond.mood = romantasy::mood::AfterPoints(bond.mood, pointsDelta, today);

    logger::debug(
        "Romantasy {}: {} point(s) for {}; total={}, rank={}",
        reason,
        pointsDelta,
        follower.name,
        follower.points,
        FactionRankForPoints(follower.points)
    );

    const auto newLevel = LevelNumberForPoints(follower.points);
    // What really happened, when it happened: the applied change (after the gate and the clamp at 0)
    // and the real tiers, whether or not the tier change is announced now. A change of 0 records nothing.
    if (const auto applied = follower.points - oldPoints; applied != 0) {
        romantasy::journal::Record(JournalFor(follower), reason, applied, oldLevel, newLevel, today);
    }
    if (newLevel != oldLevel) {
        logger::info(
            "Romantasy {} moved from {} to {} ({} point(s))",
            follower.name,
            LevelNameForPoints(oldPoints),
            LevelNameForPoints(follower.points),
            follower.points
        );
    }
    // A lost tier weighs on mood whether or not anyone is told about it.
    if (newLevel < oldLevel) {
        bond.mood = romantasy::mood::Nudged(bond.mood, romantasy::mood::kTierLostNudge, today);
    }
    if (showLevelUp && newLevel != oldLevel) {
        HoldOrAnnounceLevelChange(follower, oldLevel, newLevel, shownDelta);
    }
    // A mood that moved into Warm or Strained, for a follower who is with the player.
    if (const auto crossing = romantasy::ui::whispers::MoodCrossing(moodBefore, romantasy::mood::StateAt(bond.mood, today));
        crossing && IsFollowerFollowing(follower)) {
        RaiseWhisper({ .kind = *crossing, .follower = SavedPointsKey(follower), .name = follower.name });
    }

    return true;
}

bool RomanceManager::AwardMoment(RomanceFollower& follower, const romantasy::ostim::MomentFacts& facts)
{
    const auto verdict = romantasy::ostim::Judge(facts);
    if (!verdict.counted) {
        if (Settings::GetSingleton().DetailedLog()) {
            logger::debug("{}", romantasy::why::MomentNothing(follower.name, romantasy::ostim::Text(verdict.reason)));
        }
        return false;
    }
    // The journal exists before the moment is noted, so the first moment's day has a journal to land in.
    romantasy::journal::Begin(JournalFor(follower), LevelNumberForPoints(follower.points), facts.today);
    romantasy::ostim::Noted(MomentsFor(follower), verdict, facts.today);
    const auto label = romantasy::ostim::Label(facts.kind);
    const auto oldPoints = follower.points;
    const auto oldLevel = LevelNumberForPoints(oldPoints);
    // Never paced. Following not required. Gated: Slow burn's gate may hold part or all of the gain.
    if (verdict.points > 0) {
        (void)AddPoints(follower, verdict.points, label, true, false, true);
    }
    const auto applied = follower.points - oldPoints;
    if (auto* senderForm = RE::TESForm::LookupByID(follower.baseFormID)) {
        SendBarkEvent(_lock, "Romantasy_OnSharedMoment", std::string(romantasy::ostim::Key(facts.kind)).c_str(),
            static_cast<float>(applied), senderForm);
    }
    // A tier change has its own popup; otherwise the points really applied whisper as a deed.
    if (applied != 0 && LevelNumberForPoints(follower.points) == oldLevel) {
        RaiseWhisper({
            .kind = romantasy::ui::whispers::Kind::Deed,
            .follower = SavedPointsKey(follower),
            .name = follower.name,
            .label = std::string(label),
            .points = applied,
        });
    }
    if (applied != 0) {
        logger::info("Romantasy {}: {} point(s) for {}", label, applied, follower.name);
        if (Settings::GetSingleton().DetailedLog()) {
            logger::debug("{}", romantasy::why::Moment(
                follower.name, applied, oldPoints, follower.points, LevelNameForPoints(follower.points)));
        }
    } else if (verdict.points == 0) {
        if (Settings::GetSingleton().DetailedLog()) {
            logger::debug("{}", romantasy::why::MomentNothing(follower.name, romantasy::ostim::Text(verdict.reason)));
        }
    } else if (Settings::GetSingleton().DetailedLog()) {
        const auto gate = GateForWhy(follower);
        logger::debug("{}", romantasy::why::MomentNothing(follower.name,
            romantasy::why::NothingText(romantasy::why::Nothing::Gated, gate.tier, gate.days, gate.needed)));
    }
    ReleaseTierChanges(false);
    SendRuntimeState("Romance records updated.");
    return applied != 0;
}

void RomanceManager::AnnounceLevelChange(
    const RomanceFollower& follower, std::int32_t oldLevel, std::int32_t newLevel, std::int32_t pointsDelta, bool whisper)
{
    QueueOrShowLevelChange(BuildLevelChangeJson(follower, oldLevel, newLevel, pointsDelta), whisper);
    if (newLevel > oldLevel) {
        if (auto* senderForm = RE::TESForm::LookupByID(follower.baseFormID)) {
            SendBarkEvent(_lock, "Romantasy_OnLevelChanged", "", static_cast<float>(newLevel), senderForm);
            const std::string levelName(LevelNameForPoints(MinimumPointsForFactionRank(static_cast<std::int8_t>(newLevel - 1))));
            SendSkyrimNetEvent(_lock, [&](std::string_view player) { return romantasy::skyrimnet::TierGainedText(follower.name, levelName, player); },
                follower.referenceFormID);
        }
    } else {
        // The name of the tier announced, which is the follower's tier when it is sent at once.
        if (auto* senderForm = RE::TESForm::LookupByID(follower.baseFormID)) {
            const std::string levelName(LevelNameForPoints(MinimumPointsForFactionRank(static_cast<std::int8_t>(newLevel - 1))));
            SendBarkEvent(_lock, "Romantasy_OnLevelLost", levelName.c_str(), static_cast<float>(newLevel), senderForm);
            SendSkyrimNetEvent(_lock, [&](std::string_view player) { return romantasy::skyrimnet::TierLostText(follower.name, levelName, player); },
                follower.referenceFormID);
        }
    }
}

void RomanceManager::HoldOrAnnounceLevelChange(
    const RomanceFollower& follower, std::int32_t oldLevel, std::int32_t newLevel, std::int32_t pointsDelta)
{
    // A change still waiting from before (the setting was "At a calm moment") folds into this one.
    const auto merged = romantasy::pacing::Merge(
        TakeWaitingTierChange(follower),
        { .oldLevel = oldLevel, .newLevel = newLevel, .pointsDelta = pointsDelta, .since = GameDay(),
          .whisper = !_quietWhispers });  // where the change came from, kept while it waits
    if (!merged) {
        logger::info("Romantasy {} is back at the tier that was waiting to be announced; nothing to announce", follower.name);
    } else if (Settings::GetSingleton().TierMoment() == romantasy::pacing::TierMoment::Immediate) {
        AnnounceLevelChange(follower, merged->oldLevel, merged->newLevel, merged->pointsDelta, merged->whisper);
    } else {
        _waitingTierChanges[SavedPointsKey(follower)] = *merged;
        logger::info(
            "Romantasy is holding {}'s tier change (level {} to {}) for a calm moment",
            follower.name,
            merged->oldLevel,
            merged->newLevel);
    }
    _hasWaitingTierChanges = !_waitingTierChanges.empty();
}

std::optional<romantasy::pacing::PendingChange> RomanceManager::TakeWaitingTierChange(const RomanceFollower& follower)
{
    const auto key = SavedPointsKey(follower);
    auto found = _waitingTierChanges.find(key);
    // A change restored under the base moves to the reference, as saved points do.
    if (found == _waitingTierChanges.end() && key != follower.baseFormID && AllowsLegacyBase(follower)) {
        found = _waitingTierChanges.find(follower.baseFormID);
    }
    if (found == _waitingTierChanges.end()) {
        return std::nullopt;
    }
    const auto change = found->second;
    _waitingTierChanges.erase(found);
    return change;
}

const RomanceFollower* RomanceManager::FindFollowerForKey(RE::FormID key) const
{
    for (const auto& follower : _followers) {
        if (SavedPointsKey(follower) == key) {
            return std::addressof(follower);
        }
    }
    // A change restored under the base of a follower now known by its reference; never for
    // one reference of a non-unique NPC that has a personality file.
    for (const auto& follower : _followers) {
        if (follower.baseFormID == key && AllowsLegacyBase(follower)) {
            return std::addressof(follower);
        }
    }
    return nullptr;
}

bool RomanceManager::AllowsLegacyBase(const RomanceFollower& follower) const
{
    // Same test MoodFor and SavedOrInitialPoints make: base-keyed state moves only to unique NPCs.
    auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(follower.baseFormID);
    return !_fileProfiles.contains(follower.baseFormID) || (npc && npc->IsUnique());
}

bool RomanceManager::IsCalmPlace(std::optional<const RE::BGSLocation*> arrivedAt) const
{
    const RE::BGSLocation* location = nullptr;
    if (arrivedAt) {
        // The location just entered; none (the wilds) is never calm.
        location = *arrivedAt;
    } else {
        const auto* player = RE::PlayerCharacter::GetSingleton();
        location = player ? player->GetCurrentLocation() : nullptr;
    }
    if (!location) {
        return false;
    }
    return std::ranges::any_of(_calmPlaceKeywords, [&](const RE::BGSKeyword* keyword) {
        return keyword && location->HasKeyword(keyword);
    });
}

void RomanceManager::ReleaseTierChanges(bool justRested, std::optional<const RE::BGSLocation*> arrivedAt)
{
    if (_waitingTierChanges.empty()) {
        return;
    }

    const romantasy::pacing::CalmFacts facts{
        .inCombat = IsPlayerInCombat(),
        .calmPlace = IsCalmPlace(arrivedAt),
        .justRested = justRested,
        .inScene = SceneHolds(),
    };
    const auto moment = Settings::GetSingleton().TierMoment();
    const auto now = GameDay();
    for (auto entry = _waitingTierChanges.begin(); entry != _waitingTierChanges.end();) {
        // A follower not in memory (a reference not loaded yet after a load) keeps its change.
        const auto* follower = FindFollowerForKey(entry->first);
        const auto profile = follower ? _fileProfiles.find(follower->baseFormID) : _fileProfiles.end();
        const auto decision = romantasy::pacing::DecideRelease(entry->second, {
            .followerKnown = follower != nullptr,
            .bondEnabled = profile == _fileProfiles.end() || profile->second.enabled,
            .currentLevel = follower ? LevelNumberForPoints(follower->points) : 0,
            .moment = moment,
            .calm = facts,
            .waitedDays = now - entry->second.since,
        });
        if (decision.what == romantasy::pacing::Release::Keep) {
            ++entry;
            continue;
        }
        entry = _waitingTierChanges.erase(entry);
        if (decision.what == romantasy::pacing::Release::Drop) {
            // A switched-off bond sends nothing; a bond back where it began has nothing to tell.
            logger::info("Romantasy dropped {}'s waiting tier change; the bond is switched off or back at its old tier", follower->name);
            continue;
        }
        // Announced for the level the bond holds now, which a change not announced may have moved.
        const auto& change = decision.change;
        logger::info(
            "Romantasy announcing {}'s tier change (level {} to {}) after {:.2f} day(s)",
            follower->name,
            change.oldLevel,
            change.newLevel,
            now - change.since);
        AnnounceLevelChange(*follower, change.oldLevel, change.newLevel, change.pointsDelta, change.whisper);
    }
    _hasWaitingTierChanges = !_waitingTierChanges.empty();
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

// Called under _lock. Reads manager members only: no game calls.
std::shared_ptr<const romantasy::PointsView> RomanceManager::BuildPointsView() const
{
    auto view = std::make_shared<romantasy::PointsView>();
    for (const auto& follower : _followers) {
        if (follower.referenceFormID != 0) {
            // First entry wins, as FindCachedFollower's search does.
            view->byReference.emplace(follower.referenceFormID, follower.points);
            if (const auto bond = _moods.find(follower.referenceFormID); bond != _moods.end()) {
                view->moodByReference.emplace(follower.referenceFormID, bond->second.mood);
            }
        }
    }
    for (const auto& [base, profile] : _fileProfiles) {
        view->profiles.emplace(base, romantasy::PointsView::Profile{ profile.enabled, (profile.startingLevel - 1) * 500 });
    }
    view->saved = _savedPoints;
    return view;
}

RomanceManager::BondMood& RomanceManager::MoodFor(const RomanceFollower& follower)
{
    const auto key = SavedPointsKey(follower);
    if (const auto found = _moods.find(key); found != _moods.end()) {
        return found->second;
    }
    // A bond first stored under its base moves to its reference, as saved points do.
    if (key != follower.baseFormID) {
        auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(follower.baseFormID);
        const bool allowLegacyBase = !_fileProfiles.contains(follower.baseFormID) || (npc && npc->IsUnique());
        if (const auto legacy = _moods.find(follower.baseFormID); allowLegacyBase && legacy != _moods.end()) {
            auto moved = legacy->second;
            _moods.erase(legacy);
            // The entry under the base may not have been looked at for a while: the gap
            // since its last look is not counted as days together.
            if (const auto today = GameDay(); romantasy::mood::detail::Finite(today)) {
                moved.company.lastSeen = today;
            }
            return _moods[key] = moved;
        }
    }
    return _moods[key];
}

romantasy::mood::Mood RomanceManager::MoodOrSettled(const RomanceFollower& follower) const
{
    // Reads without creating an entry; a bond with none reads Settled.
    const auto found = _moods.find(SavedPointsKey(follower));
    return found != _moods.end() ? found->second.mood : romantasy::mood::Mood{};
}

romantasy::journal::Journal& RomanceManager::JournalFor(const RomanceFollower& follower)
{
    const auto key = SavedPointsKey(follower);
    if (const auto found = _journals.find(key); found != _journals.end()) {
        return found->second;
    }
    // A journal first stored under its base moves to its reference, as saved points and moods do.
    if (key != follower.baseFormID) {
        auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(follower.baseFormID);
        const bool allowLegacyBase = !_fileProfiles.contains(follower.baseFormID) || (npc && npc->IsUnique());
        if (const auto legacy = _journals.find(follower.baseFormID); allowLegacyBase && legacy != _journals.end()) {
            auto moved = std::move(legacy->second);
            _journals.erase(legacy);
            return _journals[key] = std::move(moved);
        }
    }
    return _journals[key];
}

romantasy::ostim::MomentState& RomanceManager::MomentsFor(const RomanceFollower& follower)
{
    const auto key = SavedPointsKey(follower);
    if (const auto found = _moments.find(key); found != _moments.end()) {
        return found->second;
    }
    // Moments first stored under the base move to the reference, as the journal does.
    if (key != follower.baseFormID) {
        auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(follower.baseFormID);
        const bool allowLegacyBase = !_fileProfiles.contains(follower.baseFormID) || (npc && npc->IsUnique());
        if (const auto legacy = _moments.find(follower.baseFormID); allowLegacyBase && legacy != _moments.end()) {
            const auto moved = legacy->second;
            _moments.erase(legacy);
            return _moments[key] = moved;
        }
    }
    return _moments[key];
}

const romantasy::ostim::MomentState* RomanceManager::FindMoments(const RomanceFollower& follower) const
{
    const auto key = SavedPointsKey(follower);
    if (const auto found = _moments.find(key); found != _moments.end()) {
        return std::addressof(found->second);
    }
    if (key != follower.baseFormID) {
        auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(follower.baseFormID);
        const bool allowLegacyBase = !_fileProfiles.contains(follower.baseFormID) || (npc && npc->IsUnique());
        if (const auto legacy = _moments.find(follower.baseFormID); allowLegacyBase && legacy != _moments.end()) {
            return std::addressof(legacy->second);
        }
    }
    return nullptr;
}

float RomanceManager::DaysTogether(const RomanceFollower& follower) const
{
    if (const auto found = _moods.find(SavedPointsKey(follower)); found != _moods.end()) {
        return found->second.company.daysTogether;
    }
    // An entry still under the base, as HasMood counts it.
    if (follower.referenceFormID != 0) {
        if (const auto legacy = _moods.find(follower.baseFormID); legacy != _moods.end()) {
            return legacy->second.company.daysTogether;
        }
    }
    return 0.0f;
}

bool RomanceManager::HasMood(const RomanceFollower& follower) const
{
    return _moods.contains(SavedPointsKey(follower)) ||
        (follower.referenceFormID != 0 && _moods.contains(follower.baseFormID));
}

// Looks at who is following and sends a reunion or time-apart event at most once per occasion.
void RomanceManager::ObserveCompany()
{
    const auto now = GameDay();
    for (const auto& follower : _followers) {
        // A switched-off bond has no mood and sends no events. Its clock still moves, so
        // the days it spends switched off are not counted as days together when it returns.
        if (const auto profile = _fileProfiles.find(follower.baseFormID);
            profile != _fileProfiles.end() && !profile->second.enabled) {
            if (HasMood(follower) && romantasy::mood::detail::Finite(now)) {
                MoodFor(follower).company.lastSeen = now;
            }
            continue;
        }
        const bool following = IsFollowerFollowing(follower);
        // A follower with no entry who is not following has nothing to record yet.
        if (!following && !HasMood(follower)) {
            continue;
        }
        auto& bond = MoodFor(follower);
        const auto step = romantasy::mood::Observe(bond.company, following, now);
        bond.company = step.next;
        if (step.event == romantasy::mood::CompanyEvent::None) {
            continue;
        }
        auto* senderForm = RE::TESForm::LookupByID(follower.baseFormID);
        if (step.event == romantasy::mood::CompanyEvent::Reunion) {
            bond.mood = romantasy::mood::Nudged(bond.mood, romantasy::mood::kReunionNudge, now);
            logger::info("Romantasy {} is back after {:.1f} day(s) apart", follower.name, step.daysApart);
            if (senderForm) {
                SendBarkEvent(_lock, "Romantasy_OnReunion", "", step.daysApart, senderForm);
                SendSkyrimNetEvent(_lock,
                    [&](std::string_view player) { return romantasy::skyrimnet::ReunionText(follower.name, WholeDays(step.daysApart), player); },
                    follower.referenceFormID);
            }
            RaiseWhisper({ .kind = romantasy::ui::whispers::Kind::Reunion, .follower = SavedPointsKey(follower), .name = follower.name });
        } else {
            logger::info("Romantasy {} has been apart for {:.1f} day(s)", follower.name, step.daysApart);
            if (senderForm) {
                SendBarkEvent(_lock, "Romantasy_OnTimeApart", "", step.daysApart, senderForm);
                SendSkyrimNetEvent(_lock,
                    [&](std::string_view player) { return romantasy::skyrimnet::TimeApartText(follower.name, WholeDays(step.daysApart), player); },
                    follower.referenceFormID);
            }
            RaiseWhisper({ .kind = romantasy::ui::whispers::Kind::TimeApart, .follower = SavedPointsKey(follower), .name = follower.name });
        }
    }
}

void RomanceManager::ApplySavedPoints()
{
    for (auto& follower : _followers) {
        follower.points = SavedOrInitialPoints(follower.referenceFormID, follower.baseFormID, follower.basePoints);
    }
}

RomanceManager::WhyGate RomanceManager::GateForWhy(const RomanceFollower& follower) const
{
    const auto days = romantasy::pacing::detail::SafeDays(DaysTogether(follower));
    const auto gate = romantasy::pacing::NextGate(follower.points, days);
    const auto level = gate ? gate->level : LevelNumberForPoints(follower.points) + 1;
    return { .tier = LevelNameForPoints(MinimumPointsForFactionRank(static_cast<std::int8_t>(level - 1))),
        .days = days,
        .needed = static_cast<std::int32_t>(romantasy::pacing::DaysNeeded(level)) };
}

void RomanceManager::LogWhyNothing(
    const RomanceFollower& follower, std::optional<std::string_view> deed, std::int32_t count, romantasy::why::Nothing reason) const
{
    if (Settings::GetSingleton().DetailedLog()) {
        const auto gate = reason == romantasy::why::Nothing::Gated ? GateForWhy(follower) : WhyGate{};
        logger::debug("{}", deed ? romantasy::why::Unchanged(follower.name, *deed, count, reason, gate.tier, gate.days, gate.needed) :
                                   romantasy::why::ScriptUnchanged(follower.name, reason, gate.tier, gate.days, gate.needed));
    }
}

bool RomanceManager::ApplyStatDelta(
    const StatFactionRule& rule, std::int32_t delta, bool showLevelUp, bool requireFollowing, bool gated, RE::FormID onlyKey)
{
    if (delta <= 0) {
        return false;
    }

    EnsureFollowersDiscovered();

    // One follower named by the caller is already refreshed; nothing new is looked for.
    if (onlyKey == 0) {
        // Refresh the followers already known, as the loaded-actor walk would: only those in
        // high process, each once. Collected first because RefreshRuntimeFollower can reorder
        // _followers.
        std::vector<RE::Actor*> knownActors;
        knownActors.reserve(_followers.size());
        for (const auto& follower : _followers) {
            auto* actor = ResolveFollowerActor(follower);
            if (!actor || std::ranges::find(knownActors, actor) != knownActors.end()) {
                continue;
            }
            const auto* process = actor->GetActorRuntimeData().currentProcess;
            if (!process || !process->InHighProcess()) {
                continue;
            }
            knownActors.push_back(actor);
        }
        for (auto* actor : knownActors) {
            (void)RefreshRuntimeFollower(actor, std::addressof(rule));
        }
        std::ranges::sort(_followers, [](const auto& lhs, const auto& rhs) {
            return lhs.name < rhs.name;
        });

        // The full walk finds followers not yet known (a new faction follower loading in). It
        // runs when the throttle is due or the player has changed cell since the last walk.
        const auto now = std::chrono::steady_clock::now();
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* playerCell = player ? player->GetParentCell() : nullptr;
        const RE::FormID cell = playerCell ? playerCell->GetFormID() : 0;
        bool walk = _actorWalk.Due(now);
        if (!walk && cell != _actorWalkCell) {
            _actorWalk.Reset();
            walk = _actorWalk.Due(now);  // re-arms: the walk is about to happen
        }
        if (walk) {
            _actorWalkCell = cell;
            RefreshLoadedFollowers(std::addressof(rule));
        }
    }
    const auto label = LabelForRule(rule);
    const auto pace = Settings::GetSingleton().Pace();
    bool changedAnyFollower = false;

    for (auto& follower : _followers) {
        const auto key = SavedPointsKey(follower);
        if (onlyKey != 0 && key != onlyKey) {
            continue;
        }
        const auto direction = follower.statDirections.find(rule.editorID);
        if (direction == follower.statDirections.end() || direction->second == 0) {
            if (Settings::GetSingleton().DetailedLog()) {
                if (const auto why = romantasy::why::Reason({ .feeling = false })) {
                    LogWhyNothing(follower, label, delta, *why);
                }
            }
            continue;
        }

        // A gain from a deed is paced; a loss passes through unchanged.
        const auto preferencePoints = romantasy::PreferencePoints(delta, rule.points, direction->second);
        const auto paced = romantasy::pacing::ApplyPace(pace, preferencePoints, _paceCarry.contains(key));
        const auto keepCarry = [&]() {
            if (paced.carry) {
                _paceCarry.insert(key);
            } else {
                _paceCarry.erase(key);
            }
        };
        const auto pointsDelta = paced.points;
        if (pointsDelta == 0) {
            // Slow burn's half point is kept for the next gain, by a bond that could have earned it.
            const bool canEarn = !requireFollowing || IsFollowerFollowing(follower);
            if (canEarn) {
                keepCarry();
            }
            if (Settings::GetSingleton().DetailedLog()) {
                if (const auto why = romantasy::why::Reason({ .points = preferencePoints, .following = canEarn, .paced = 0 })) {
                    LogWhyNothing(follower, label, delta, *why);
                }
            }
            continue;
        }
        const auto beforeLevel = LevelNumberForPoints(follower.points);
        const auto beforePoints = follower.points;
        // The deed's result by the pure rule, read before the change; AddPoints applies the same gate.
        const auto award = romantasy::pacing::AwardForStat(
            beforePoints, preferencePoints, pace, _paceCarry.contains(key), DaysTogether(follower), gated);
        const bool changed = AddPoints(follower, pointsDelta, label, showLevelUp, requireFollowing, gated);
        if (Settings::GetSingleton().DetailedLog()) {
            const auto why = romantasy::why::Reason({
                .points = preferencePoints,
                .following = changed || !requireFollowing || IsFollowerFollowing(follower),
                .paced = pointsDelta,
                .refused = !changed && award.refused,
                .applied = follower.points - beforePoints,
            });
            if (why) {
                LogWhyNothing(follower, label, delta, *why);
            } else {
                const auto paceSign = pace == romantasy::pacing::Pace::Slow ? -1 : pace == romantasy::pacing::Pace::Fast ? 1 : 0;
                logger::debug("{}", romantasy::why::Changed({
                    .name = follower.name,
                    .deed = label,
                    .count = delta,
                    .rule = rule.points,
                    .weight = direction->second,
                    .pace = romantasy::why::PaceShown(paceSign, preferencePoints),
                    .applied = follower.points - beforePoints,
                    .oldPoints = beforePoints,
                    .newPoints = follower.points,
                    .tier = LevelNameForPoints(follower.points),
                }));
            }
        }
        // A gate holds points, not the follower's reaction: a deed refused in full by a Slow burn
        // gate (not by the following rule) is still felt, reported at the paced amount it was worth.
        const bool heldAtGate = !changed && award.refused && (!requireFollowing || IsFollowerFollowing(follower));
        if (changed) {
            keepCarry();
            changedAnyFollower = true;
        }
        if (changed || heldAtGate) {
            // Deterministic dedup: AddPoints already sent (or holds for a calm moment)
            // OnLevelChanged if this tick crossed a tier. Only fire the preference event when it
            // did NOT, so a single tick never produces two barks.
            if (LevelNumberForPoints(follower.points) == beforeLevel && award.reported != 0) {
                if (auto* senderForm = RE::TESForm::LookupByID(follower.baseFormID)) {
                    // A gain reports what was awarded (a gate may hold part of it); a loss what was asked, as before.
                    SendBarkEvent(_lock, "Romantasy_OnPreference", label.c_str(), static_cast<float>(award.reported), senderForm);
                }
                // The whisper shows only the points really applied: none when a gate held the whole gain.
                if (const auto applied = follower.points - beforePoints; applied != 0 && IsFollowerFollowing(follower)) {
                    RaiseWhisper({
                        .kind = romantasy::ui::whispers::Kind::Deed,
                        .follower = key,
                        .name = follower.name,
                        .label = label,
                        .points = applied,
                        .strong = direction->second == 2 || direction->second == -2,
                    });
                }
            }
        }
    }

    if (changedAnyFollower) {
        SendRuntimeState("Romance records updated.");
    }
    return changedAnyFollower;
}

void RomanceManager::EnsureFollowersDiscovered()
{
    if (_discoveryDone) {
        return;
    }

    _followers = DiscoverFollowers();
    _discoveryDone = true;
    ApplySavedPoints();
    MirrorRelationshipLevels();
    logger::info("Romantasy refreshed follower cache: {} romanceable NPC base records", _followers.size());
}

void RomanceManager::FlushPendingLevelChanges()
{
    if (_pendingLevelChanges.empty() || IsPlayerInCombat() || SceneHolds()) {
        return;
    }

    logger::debug("Romantasy showing {} pending relationship level notification(s) after combat or a scene", _pendingLevelChanges.size());
    while (!_pendingLevelChanges.empty()) {
        auto pending = std::move(_pendingLevelChanges.front());
        _pendingLevelChanges.pop_front();
        _lock.Defer([payload = std::move(pending.payload), whisper = pending.whisper,
                        generation = RomantasyUI::GetSingleton().WhisperGeneration()]() {
            RomantasyUI::GetSingleton().ShowLevelUpModal(payload, whisper, generation);
        });
    }
    _hasPendingLevelChanges = false;
}

void RomanceManager::Load(SKSE::SerializationInterface* serialization)
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    _savedPoints.clear();
    _authorIntegrationBaseForms.clear();
    _relationshipRankMirrors.clear();
    _relationshipPointsMirrors.clear();
    _manualPreferenceActors.clear();
    _playerEnrolledActors.clear();
    _orphanedPoints.clear();
    _orphanedBonds.clear();
    _moods.clear();
    _orphanedMoods.clear();
    _waitingTierChanges.clear();
    _journals.clear();
    _orphanedJournals.clear();
    _moments.clear();
    _orphanedMoments.clear();
    _paceCarry.clear();
    _conversations.clear();
    _loadDamaged = false;

    using romantasy::save::IdentityState;
    using romantasy::save::Restore;
    const auto classify = [&](const romantasy::save::FormIdentity& identity) {
        // A dynamic ID passes through ResolveFormID unchanged, so its resolving says
        // nothing about a plugin; only a named identity is checked against the load order.
        if (identity.plugin.empty()) {
            return identity.rawFormID == 0 ?
                std::pair{ IdentityState::Empty, std::optional<RE::FormID>{} } :
                std::pair{ IdentityState::Dynamic, ResolveIdentity(identity, serialization) };
        }
        const auto formID = ResolveIdentity(identity, serialization);
        return std::pair{ formID ? IdentityState::NamedResolved : IdentityState::NamedUnresolved, formID };
    };

    // Applied after every record is read, before the legacy migration and discovery below.
    std::optional<romantasy::save::ReadResult<romantasy::save::PlayerBond>> savedBonds;
    std::uint32_t type = 0;
    std::uint32_t version = 0;
    std::uint32_t length = 0;
    while (serialization->GetNextRecordInfo(type, version, length)) {
        if (type == kPlayerBondsRecord) {
            if (version != romantasy::save::kPlayerBondsVersion) {
                logger::warn("Romantasy skipping player-bond record version {}", version);
                continue;
            }
            SkseReader reader(serialization);
            savedBonds = romantasy::save::ReadPlayerBonds(reader, length);
            if (!savedBonds->complete) {
                logger::error(
                    "Romantasy player-bond record is damaged; kept the {} complete entr(ies) before the damage",
                    savedBonds->entries.size());
                _loadDamaged = true;
            }
            continue;
        }

        if (type == kPlayerEnrollmentRecord) {
            if (version != kPlayerEnrollmentVersion) {
                logger::warn("Romantasy skipping player-enrollment record version {}", version);
                continue;
            }

            std::uint32_t count = 0;
            if (!ReadRecord(serialization, count)) {
                logger::error("Romantasy failed to read player-enrollment identity count");
                _loadDamaged = true;
                continue;
            }
            for (std::uint32_t index = 0; index < count; ++index) {
                RE::FormID serializedFormID = 0;
                if (!ReadRecord(serialization, serializedFormID)) {
                    logger::error("Romantasy failed to read player-enrollment identity {}", index);
                    _loadDamaged = true;
                    break;
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
                _loadDamaged = true;
                continue;
            }

            for (std::uint32_t index = 0; index < count; ++index) {
                RE::FormID serializedFormID = 0;
                if (!ReadRecord(serialization, serializedFormID)) {
                    logger::error("Romantasy failed to read manual-preference identity {}", index);
                    _loadDamaged = true;
                    break;
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

        if (type == kMoodsRecord) {
            if (version != romantasy::save::kMoodsVersion) {
                logger::warn("Romantasy skipping mood record version {}", version);
                continue;
            }
            SkseReader reader(serialization);
            const auto result = romantasy::save::ReadMoods(reader, length);
            if (!result.complete) {
                logger::error(
                    "Romantasy mood record is damaged; kept the {} complete entr(ies) before the damage",
                    result.entries.size());
                _loadDamaged = true;
            }
            for (const auto& entry : result.entries) {
                const auto [referenceState, reference] = classify(entry.reference);
                const auto [baseState, base] = classify(entry.base);
                const auto decision = romantasy::save::DecideRestore(referenceState, baseState);
                std::optional<RE::FormID> key;
                if (decision == Restore::UseReference && reference) {
                    key = reference;
                } else if ((decision == Restore::UseBase || decision == Restore::UseReference) && base) {
                    key = base;
                }
                if (key) {
                    _moods[*key] = { entry.mood, entry.company };
                    continue;
                }
                if (decision != Restore::Orphan) {
                    continue;
                }
                // Kept as read, like orphaned points, so days together survive a session without the plugin.
                auto orphan = entry;
                if (orphan.reference.plugin.empty()) {
                    orphan.reference = {};
                }
                if (orphan.base.plugin.empty()) {
                    orphan.base = {};
                }
                _orphanedMoods.push_back(std::move(orphan));
            }
            continue;
        }

        if (type == kPendingRecord) {
            if (version != romantasy::save::kPendingVersion) {
                logger::warn("Romantasy skipping waiting tier change record version {}", version);
                continue;
            }
            SkseReader reader(serialization);
            const auto result = romantasy::save::ReadPending(reader, length);
            if (!result.complete) {
                logger::error(
                    "Romantasy waiting tier change record is damaged; kept the {} complete entr(ies) before the damage",
                    result.entries.size());
                _loadDamaged = true;
            }
            for (const auto& entry : result.entries) {
                const auto [referenceState, reference] = classify(entry.reference);
                const auto [baseState, base] = classify(entry.base);
                const auto decision = romantasy::save::DecideRestore(referenceState, baseState);
                if (decision == Restore::UseReference && reference) {
                    _waitingTierChanges[*reference] = entry.change;
                } else if ((decision == Restore::UseBase || decision == Restore::UseReference) && base) {
                    _waitingTierChanges[*base] = entry.change;
                } else {
                    // An announcement is not worth keeping for a follower whose plugin is gone.
                    logger::info(
                        "Romantasy dropped a waiting tier change for {:08X}; its follower is not loaded",
                        entry.reference.rawFormID != 0 ? entry.reference.rawFormID : entry.base.rawFormID);
                }
            }
            continue;
        }

        if (type == kJournalsRecord) {
            if (version != romantasy::save::kJournalsVersion) {
                logger::warn("Romantasy skipping journal record version {}", version);
                continue;
            }
            SkseReader reader(serialization);
            auto result = romantasy::save::ReadJournals(reader, length);
            if (!result.complete) {
                logger::error(
                    "Romantasy journal record is damaged; kept the {} complete entr(ies) before the damage",
                    result.entries.size());
                _loadDamaged = true;
            }
            for (auto& entry : result.entries) {
                // A save's journal is not trusted: it is repaired into one the rules could have produced.
                entry.journal = romantasy::journal::Normalize(std::move(entry.journal));
                const auto [referenceState, reference] = classify(entry.reference);
                const auto [baseState, base] = classify(entry.base);
                const auto decision = romantasy::save::DecideRestore(referenceState, baseState);
                std::optional<RE::FormID> key;
                if (decision == Restore::UseReference && reference) {
                    key = reference;
                } else if ((decision == Restore::UseBase || decision == Restore::UseReference) && base) {
                    key = base;
                }
                if (key) {
                    _journals[*key] = std::move(entry.journal);
                    continue;
                }
                if (decision != Restore::Orphan) {
                    continue;
                }
                // Normalised above, then kept and written back by every Save, like orphaned points,
                // so the journal survives a session without the plugin.
                auto orphan = std::move(entry);
                if (orphan.reference.plugin.empty()) {
                    orphan.reference = {};
                }
                if (orphan.base.plugin.empty()) {
                    orphan.base = {};
                }
                _orphanedJournals.push_back(std::move(orphan));
            }
            continue;
        }

        if (type == kMomentsRecord) {
            if (version != romantasy::save::kMomentsVersion) {
                logger::warn("Romantasy skipping moments record version {}", version);
                continue;
            }
            SkseReader reader(serialization);
            auto result = romantasy::save::ReadMoments(reader, length);
            if (!result.complete) {
                logger::error(
                    "Romantasy moments record is damaged; kept the {} complete entr(ies) before the damage",
                    result.entries.size());
                _loadDamaged = true;
            }
            for (auto& entry : result.entries) {
                const auto [referenceState, reference] = classify(entry.reference);
                const auto [baseState, base] = classify(entry.base);
                const auto decision = romantasy::save::DecideRestore(referenceState, baseState);
                std::optional<RE::FormID> key;
                if (decision == Restore::UseReference && reference) {
                    key = reference;
                } else if ((decision == Restore::UseBase || decision == Restore::UseReference) && base) {
                    key = base;
                }
                if (key) {
                    _moments[*key] = entry.state;
                    continue;
                }
                if (decision != Restore::Orphan) {
                    continue;
                }
                // Kept as read and written back by every Save, like orphaned journals,
                // so the moments survive a session without the plugin.
                auto orphan = std::move(entry);
                if (orphan.reference.plugin.empty()) {
                    orphan.reference = {};
                }
                if (orphan.base.plugin.empty()) {
                    orphan.base = {};
                }
                _orphanedMoments.push_back(std::move(orphan));
            }
            continue;
        }

        if (type != kRomancePointsRecord) {
            logger::warn("Romantasy skipping unknown serialization record type {:08X}", type);
            continue;
        }

        if (version == romantasy::save::kPointsVersion) {
            SkseReader reader(serialization);
            const auto result = romantasy::save::ReadPoints(reader, length);
            if (!result.complete) {
                logger::error(
                    "Romantasy romance point record is damaged; kept the {} complete entr(ies) before the damage",
                    result.entries.size());
                _loadDamaged = true;
            }
            for (const auto& entry : result.entries) {
                const auto [referenceState, reference] = classify(entry.reference);
                const auto [baseState, base] = classify(entry.base);
                const auto decision = romantasy::save::DecideRestore(referenceState, baseState);
                if (decision == Restore::UseReference && reference) {
                    _savedPoints[*reference] = entry.points;
                    continue;
                }
                // A dynamic reference that SKSE did not resolve falls back to a usable base, as before.
                if ((decision == Restore::UseBase || decision == Restore::UseReference) && base) {
                    _savedPoints[*base] = entry.points;
                    if (referenceState != IdentityState::Empty) {
                        logger::warn(
                            "Romantasy could not resolve saved follower reference {:08X}; retained points under base {:08X}",
                            entry.reference.rawFormID,
                            *base);
                    }
                    continue;
                }
                if (decision != Restore::Orphan) {
                    // A raw FormID cannot be carried into another save safely: a later load
                    // order could map it onto a different form. Dropped, as 3.0.x did.
                    logger::warn(
                        "Romantasy could not resolve saved follower reference {:08X} or base {:08X}",
                        entry.reference.rawFormID,
                        entry.base.rawFormID);
                    continue;
                }
                // Name the identity that carries the bond: the base when it is named, else the reference.
                const bool byBase = !entry.base.plugin.empty();
                const auto& named = byBase ? entry.base : entry.reference;
                logger::warn(
                    "Romantasy is keeping {} point(s) for follower {} {:06X} in '{}'; its plugin is not loaded",
                    entry.points,
                    byBase ? "base" : "reference",
                    named.localID,
                    named.plugin);
                auto orphan = entry;
                // A raw FormID could resolve onto an unrelated form in a later save; the named
                // identity carries the bond.
                if (orphan.reference.plugin.empty()) {
                    orphan.reference = {};
                }
                if (orphan.base.plugin.empty()) {
                    orphan.base = {};
                }
                _orphanedPoints.push_back(std::move(orphan));
            }
            continue;
        }

        if (version != 1 && version != 2) {
            logger::warn("Romantasy skipping romance point record version {}", version);
            continue;
        }

        std::uint32_t count = 0;
        if (!ReadRecord(serialization, count)) {
            logger::error("Romantasy failed to read serialized romance point count");
            _loadDamaged = true;
            continue;
        }

        for (std::uint32_t index = 0; index < count; ++index) {
            if (version == 1) {
                SerializedRomancePointsV1 serialized;
                if (!ReadRecord(serialization, serialized)) {
                    logger::error("Romantasy failed to read version 1 romance point entry {}", index);
                    _loadDamaged = true;
                    break;
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
                _loadDamaged = true;
                break;
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

    if (savedBonds) {
        ApplyPlayerBonds(*savedBonds, serialization);
    } else {
        // No record (a 3.0.x save): start from the files on disk, including those whose
        // plugin is not loaded, without relying on Revert having run first.
        UseLibraryBonds();
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
    _discoveryDone = true;
    _actorWalk.Reset();
    _actorWalkCell = 0;
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
    _hasWaitingTierChanges = !_waitingTierChanges.empty();
    logger::info(
        "Romantasy loaded {} serialized romance point entries, {} player-managed preference flag(s), {} player enrollment(s), and kept {} bond(s) whose plugin is not loaded",
        _savedPoints.size(),
        _manualPreferenceActors.size(),
        _playerEnrolledActors.size(),
        _orphanedPoints.size());
}

std::size_t RomanceManager::KeptBondsCount() const
{
    // One number for the load HUD line and the health note: a bond can be kept with points, without, or both.
    romantasy::OutboxLock::Scope lock(_lock);
    return (std::max)(_orphanedPoints.size(), _orphanedBonds.size());
}

void RomanceManager::FillHealth(romantasy::health::Facts& facts) const
{
    romantasy::OutboxLock::Scope lock(_lock);
    facts.romanceLevelFaction = _romanceLevelFaction != nullptr;
    facts.followerFaction = _currentFollowerFaction != nullptr;
    facts.statFactions = static_cast<int>(_statFactions.size());
    facts.saveDamaged = _loadDamaged;
    facts.orphans = static_cast<int>(KeptBondsCount());
    // What the ledger lists: every follower whose personality file is not switched off.
    facts.bonds = static_cast<int>(std::ranges::count_if(_followers, [&](const RomanceFollower& follower) {
        const auto profile = _fileProfiles.find(follower.baseFormID);
        return profile == _fileProfiles.end() || profile->second.enabled;
    }));
    facts.profileProblems = _profileProblems;
}

void RomanceManager::MirrorRelationshipLevel(RomanceFollower& follower)
{
    if (_fileProfiles.contains(follower.baseFormID) || !_romanceLevelFaction) {
        return;
    }

    auto* actor = ResolveFollowerActor(follower);
    if (!actor) {
        logger::debug(
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
    logger::debug("Romantasy mirrored {} to ROM_RomanceLevel rank {}", follower.name, rank);
}

void RomanceManager::MirrorRelationshipLevels()
{
    for (auto& follower : _followers) {
        MirrorRelationshipLevel(follower);
    }
}

void RomanceManager::QueueOrShowLevelChange(nlohmann::json payload, bool whisper)
{
    if (IsPlayerInCombat() || SceneHolds()) {
        logger::debug(
            "Romantasy queued relationship level {} notification for {}; player is in combat or a scene",
            payload.value("changeDirection", "change"),
            payload.value("followerName", "unknown follower")
        );
        _pendingLevelChanges.push_back({ std::move(payload), whisper });
        _hasPendingLevelChanges = true;
        // Combat may have ended before the push, and the combat sink may have skipped
        // the flush while the flag was still false.
        if (!IsPlayerInCombat() && !SceneHolds()) {
            FlushPendingLevelChanges();
        }
        return;
    }

    _lock.Defer([payload = std::move(payload), whisper, generation = RomantasyUI::GetSingleton().WhisperGeneration()]() {
        RomantasyUI::GetSingleton().ShowLevelUpModal(payload, whisper, generation);
    });
}

void RomanceManager::RaiseWhisper(romantasy::ui::whispers::Whisper whisper)
{
    // Only shows: queued for the game thread after the lock is released, never changing what the manager holds.
    if (_quietWhispers || !Settings::GetSingleton().Whispers()) {
        return;
    }
    // Stamped with the game it was raised in: one handed over after a load is dropped.
    _lock.Defer([whisper = std::move(whisper), generation = RomantasyUI::GetSingleton().WhisperGeneration()]() mutable {
        RomantasyUI::GetSingleton().Whisper(std::move(whisper), generation);
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

        if (!_calmSinksRegistered) {
            events->AddEventSink<RE::TESActorLocationChangeEvent>(this);
            events->AddEventSink<RE::TESSleepStopEvent>(this);
            events->AddEventSink<RE::TESWaitStopEvent>(this);
            _calmSinksRegistered = true;
            logger::info("Romantasy location, sleep and wait sinks registered");
        }
    } else {
        logger::critical("Romantasy failed to register event sinks");
    }

    if (!_menuSinkRegistered) {
        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
            _menuSinkRegistered = true;
            logger::info("Romantasy dialogue menu sink registered");
        } else {
            logger::error("Romantasy failed to register the dialogue menu sink");
        }
    }
}

void RomanceManager::Revert()
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    _lastStatValues.clear();
    _savedPoints.clear();
    _authorIntegrationBaseForms.clear();
    _relationshipRankMirrors.clear();
    _relationshipPointsMirrors.clear();
    _manualPreferenceActors.clear();
    _playerEnrolledActors.clear();
    _orphanedPoints.clear();
    _orphanedBonds.clear();
    _moods.clear();
    _orphanedMoods.clear();
    _waitingTierChanges.clear();
    _hasWaitingTierChanges = false;
    _journals.clear();
    _orphanedJournals.clear();
    _moments.clear();
    _orphanedMoments.clear();
    _paceCarry.clear();
    _conversations.clear();
    _loadDamaged = false;
    _pendingLevelChanges.clear();
    _hasPendingLevelChanges = false;
    UseLibraryBonds();
    _followers = DiscoverFollowers();
    _discoveryDone = true;
    _actorWalk.Reset();
    _actorWalkCell = 0;
    MirrorRelationshipLevels();
    logger::info("Romantasy serialization state reverted; rebuilt {} romanceable follower definition(s)", _followers.size());
}

bool RomanceManager::Save(SKSE::SerializationInterface* serialization) const
{
    romantasy::OutboxLock::Scope lock(_lock);
    // Each record is written independently; one failing does not skip the others.
    const bool points = SavePoints(serialization);
    const bool moods = SaveMoods(serialization);
    const bool preferences = SaveManualPreferences(serialization);
    const bool enrollments = SavePlayerEnrollments(serialization);
    const bool bonds = SavePlayerBonds(serialization);
    const bool pending = SavePending(serialization);
    const bool journals = SaveJournals(serialization);
    const bool moments = SaveMoments(serialization);
    return points && moods && preferences && enrollments && bonds && pending && journals && moments;
}

void RomanceManager::ClearLoadDamaged()
{
    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    _loadDamaged = false;
}

bool RomanceManager::SavePoints(SKSE::SerializationInterface* serialization) const
{
    if (!serialization->OpenRecord(kRomancePointsRecord, romantasy::save::kPointsVersion)) {
        logger::error("Romantasy failed to open romance point serialization record");
        return false;
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

    std::vector<romantasy::save::PointsEntry> entries;
    entries.reserve(serializedByIdentity.size() + _orphanedPoints.size());
    for (const auto& [identityFormID, serialized] : serializedByIdentity) {
        entries.push_back({
            .reference = IdentityForFormID(serialized.referenceFormID),
            .base = IdentityForFormID(serialized.baseFormID),
            .points = serialized.points,
        });
    }
    entries.insert(entries.end(), _orphanedPoints.begin(), _orphanedPoints.end());

    SkseWriter writer(serialization);
    if (!romantasy::save::WritePoints(writer, entries)) {
        logger::error("Romantasy failed to write romance point entries");
        return false;
    }

    logger::info(
        "Romantasy saved {} romance point entries ({} kept for followers whose plugin is not loaded)",
        entries.size(),
        _orphanedPoints.size());
    return true;
}

bool RomanceManager::SaveMoods(SKSE::SerializationInterface* serialization) const
{
    if (!serialization->OpenRecord(kMoodsRecord, romantasy::save::kMoodsVersion)) {
        logger::error("Romantasy failed to open mood serialization record");
        return false;
    }

    std::vector<romantasy::save::MoodEntry> entries;
    entries.reserve(_moods.size() + _orphanedMoods.size());
    for (const auto& [identityFormID, bond] : _moods) {
        // Identities as the points record builds them: a reference with its base, else a base.
        RE::FormID referenceFormID = 0;
        RE::FormID baseFormID = identityFormID;
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(identityFormID)) {
            referenceFormID = identityFormID;
            auto* baseNpc = actor->GetActorBase();
            baseFormID = baseNpc ? baseNpc->GetFormID() : 0;
        }
        entries.push_back({
            .reference = IdentityForFormID(referenceFormID),
            .base = IdentityForFormID(baseFormID),
            .mood = bond.mood,
            .company = bond.company,
        });
    }
    entries.insert(entries.end(), _orphanedMoods.begin(), _orphanedMoods.end());

    SkseWriter writer(serialization);
    if (!romantasy::save::WriteMoods(writer, entries)) {
        logger::error("Romantasy failed to write mood entries");
        return false;
    }

    logger::info(
        "Romantasy saved {} mood entries ({} kept for followers whose plugin is not loaded)",
        entries.size(),
        _orphanedMoods.size());
    return true;
}

bool RomanceManager::SaveManualPreferences(SKSE::SerializationInterface* serialization) const
{
    if (!serialization->OpenRecord(kManualPreferencesRecord, kManualPreferencesVersion)) {
        logger::error("Romantasy failed to open manual-preference serialization record");
        return false;
    }

    const auto manualCount = static_cast<std::uint32_t>(_manualPreferenceActors.size());
    if (!serialization->WriteRecordData(manualCount)) {
        logger::error("Romantasy failed to write manual-preference identity count");
        return false;
    }

    for (const auto identityFormID : _manualPreferenceActors) {
        if (!serialization->WriteRecordData(identityFormID)) {
            logger::error("Romantasy failed to write manual-preference identity {:08X}", identityFormID);
            return false;
        }
    }

    logger::info("Romantasy saved {} player-managed preference flag(s)", manualCount);
    return true;
}

bool RomanceManager::SavePlayerEnrollments(SKSE::SerializationInterface* serialization) const
{
    if (!serialization->OpenRecord(kPlayerEnrollmentRecord, kPlayerEnrollmentVersion)) {
        logger::error("Romantasy failed to open player-enrollment serialization record");
        return false;
    }

    const auto enrollmentCount = static_cast<std::uint32_t>(_playerEnrolledActors.size());
    if (!serialization->WriteRecordData(enrollmentCount)) {
        logger::error("Romantasy failed to write player-enrollment identity count");
        return false;
    }
    for (const auto identityFormID : _playerEnrolledActors) {
        if (!serialization->WriteRecordData(identityFormID)) {
            logger::error("Romantasy failed to write player-enrollment identity {:08X}", identityFormID);
            return false;
        }
    }
    logger::info("Romantasy saved {} player enrollment(s)", enrollmentCount);
    return true;
}

bool RomanceManager::SavePlayerBonds(SKSE::SerializationInterface* serialization) const
{
    if (!serialization->OpenRecord(kPlayerBondsRecord, romantasy::save::kPlayerBondsVersion)) {
        logger::error("Romantasy failed to open player-bond serialization record");
        return false;
    }

    std::vector<romantasy::save::PlayerBond> bonds;
    for (const auto& [formID, profile] : _fileProfiles) {
        if (!profile.playerOwned) {
            continue;
        }
        romantasy::save::PlayerBond bond;
        bond.base = { .plugin = profile.plugin, .localID = profile.localID, .rawFormID = formID };
        bond.enabled = profile.enabled;
        for (const auto& [statName, direction] : profile.preferences) {
            bond.preferences.emplace_back(statName, static_cast<std::int8_t>(direction));
        }
        std::ranges::sort(bond.preferences);
        bonds.push_back(std::move(bond));
    }
    const auto liveCount = bonds.size();
    bonds.insert(bonds.end(), _orphanedBonds.begin(), _orphanedBonds.end());

    SkseWriter writer(serialization);
    if (!romantasy::save::WritePlayerBonds(writer, bonds)) {
        logger::error("Romantasy failed to write player-bond entries");
        return false;
    }

    logger::info(
        "Romantasy saved {} player bond(s) ({} kept for NPCs that are unavailable or mod-owned)",
        bonds.size(),
        bonds.size() - liveCount);
    return true;
}

bool RomanceManager::SavePending(SKSE::SerializationInterface* serialization) const
{
    if (!serialization->OpenRecord(kPendingRecord, romantasy::save::kPendingVersion)) {
        logger::error("Romantasy failed to open waiting tier change serialization record");
        return false;
    }

    std::vector<romantasy::save::PendingEntry> entries;
    entries.reserve(_waitingTierChanges.size());
    for (const auto& [identityFormID, change] : _waitingTierChanges) {
        // Identities as the points record builds them: a reference with its base, else a base.
        RE::FormID referenceFormID = 0;
        RE::FormID baseFormID = identityFormID;
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(identityFormID)) {
            referenceFormID = identityFormID;
            auto* baseNpc = actor->GetActorBase();
            baseFormID = baseNpc ? baseNpc->GetFormID() : 0;
        }
        entries.push_back({
            .reference = IdentityForFormID(referenceFormID),
            .base = IdentityForFormID(baseFormID),
            .change = change,
        });
    }

    SkseWriter writer(serialization);
    if (!romantasy::save::WritePending(writer, entries)) {
        logger::error("Romantasy failed to write waiting tier change entries");
        return false;
    }

    logger::info("Romantasy saved {} waiting tier change(s)", entries.size());
    return true;
}

bool RomanceManager::SaveJournals(SKSE::SerializationInterface* serialization) const
{
    if (!serialization->OpenRecord(kJournalsRecord, romantasy::save::kJournalsVersion)) {
        logger::error("Romantasy failed to open journal serialization record");
        return false;
    }

    std::vector<romantasy::save::JournalEntry> entries;
    entries.reserve(_journals.size() + _orphanedJournals.size());
    for (const auto& [identityFormID, journal] : _journals) {
        // Identities as the points record builds them: a reference with its base, else a base.
        RE::FormID referenceFormID = 0;
        RE::FormID baseFormID = identityFormID;
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(identityFormID)) {
            referenceFormID = identityFormID;
            auto* baseNpc = actor->GetActorBase();
            baseFormID = baseNpc ? baseNpc->GetFormID() : 0;
        }
        entries.push_back({
            .reference = IdentityForFormID(referenceFormID),
            .base = IdentityForFormID(baseFormID),
            // Rules-made journals come back unchanged; anything else is repaired so one journal never costs the record.
            .journal = romantasy::journal::Normalize(journal),
        });
    }
    entries.insert(entries.end(), _orphanedJournals.begin(), _orphanedJournals.end());

    SkseWriter writer(serialization);
    if (!romantasy::save::WriteJournals(writer, entries)) {
        logger::error("Romantasy failed to write journal entries");
        return false;
    }

    logger::info(
        "Romantasy saved {} journal(s) ({} kept for followers whose plugin is not loaded)",
        entries.size(),
        _orphanedJournals.size());
    return true;
}

bool RomanceManager::SaveMoments(SKSE::SerializationInterface* serialization) const
{
    if (!serialization->OpenRecord(kMomentsRecord, romantasy::save::kMomentsVersion)) {
        logger::error("Romantasy failed to open moments serialization record");
        return false;
    }

    std::vector<romantasy::save::MomentEntry> entries;
    entries.reserve(_moments.size() + _orphanedMoments.size());
    std::size_t written = 0;
    for (const auto& [identityFormID, state] : _moments) {
        // An entry the writer would refuse is left out, so one bad entry never costs the record.
        if (!romantasy::save::MomentFits(state)) {
            logger::warn("Romantasy left out the shared moments of {:08X}; a day was not valid", identityFormID);
            continue;
        }
        // Identities as the points record builds them: a reference with its base, else a base.
        RE::FormID referenceFormID = 0;
        RE::FormID baseFormID = identityFormID;
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(identityFormID)) {
            referenceFormID = identityFormID;
            auto* baseNpc = actor->GetActorBase();
            baseFormID = baseNpc ? baseNpc->GetFormID() : 0;
        }
        entries.push_back({
            .reference = IdentityForFormID(referenceFormID),
            .base = IdentityForFormID(baseFormID),
            .state = state,
        });
    }
    entries.insert(entries.end(), _orphanedMoments.begin(), _orphanedMoments.end());
    // The writer leaves empty entries out; the log counts what is written.
    for (const auto& entry : entries) {
        written += romantasy::save::MomentEmpty(entry.state) ? 0 : 1;
    }

    SkseWriter writer(serialization);
    if (!romantasy::save::WriteMoments(writer, entries)) {
        logger::error("Romantasy failed to write moments entries");
        return false;
    }

    logger::info(
        "Romantasy saved shared moments for {} bond(s) ({} kept for followers whose plugin is not loaded)",
        written,
        _orphanedMoments.size());
    return true;
}

void RomanceManager::LoadCallback(SKSE::SerializationInterface* serialization)
{
    auto& manager = GetSingleton();
    manager.Load(serialization);
    // Load has released _lock here; queue the HUD line outside it.
    const auto orphans = manager.KeptBondsCount();
    // Shown once per game session, and again when the number changes; the log has it on every load.
    static std::size_t lastShownOrphans = 0;
    if (orphans > 0 && orphans != lastShownOrphans) {
        lastShownOrphans = orphans;
        auto message = std::format(
            "Romantasy is keeping {} bond(s) for followers whose mods are not loaded.",
            orphans);
        SKSE::GetTaskInterface()->AddTask([message = std::move(message)]() {
            RE::SendHUDMessage::ShowHUDMessage(message.c_str());
        });
    }
}

void RomanceManager::RevertCallback(SKSE::SerializationInterface*)
{
    GetSingleton().Revert();
}

void RomanceManager::SaveCallback(SKSE::SerializationInterface* serialization)
{
    auto& manager = GetSingleton();
    bool saved = false;
    {
        // Look at who is following first, in the same scope as the write, so a change made just
        // before saving is in the file and loading it does not send reunion or time apart again.
        // Events are deferred and queued only after this scope releases the lock.
        romantasy::OutboxLock::Scope lock(manager._lock);
        manager._viewDirty = true;
        try {
            manager.ObserveCompany();
        } catch (const std::exception& e) {
            // The look is a courtesy to the save; the records are written either way.
            logger::warn("Romantasy could not look at companions before saving: {}", e.what());
        }
        saved = manager.Save(serialization);
    }
    // A save that wrote every record repairs what the last load found damaged; a failed write does not.
    if (saved) {
        manager.ClearLoadDamaged();
    }
}

void RomanceManager::SendRuntimeState(std::string_view message)
{
    if (!RomantasyUI::GetSingleton().IsOpen()) {
        return;
    }

    const auto state = BuildStateJson(message);
    _lock.Defer([state]() {
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

    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    ObserveCompany();
    const std::string statName = event->stat.c_str();
    const auto rule = _statRulesByName.find(statName);
    const auto currentValue = event->value;
    if (rule != _statRulesByName.end() && currentValue >= 0) {
        std::int32_t delta = 1;
        if (const auto previous = _lastStatValues.find(statName); previous != _lastStatValues.end()) {
            delta = currentValue - previous->second;
        }
        _lastStatValues[statName] = currentValue;

        if (delta > 0) {
            ApplyStatDelta(*rule->second, delta);
        }
    }

    // Every statistic event is a moment to look for calm, including one that just changed a tier.
    ReleaseTierChanges(false);
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl RomanceManager::ProcessEvent(
    const RE::TESCombatEvent* event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESCombatEvent>* eventSource)
{
    if (!event || (!_hasPendingLevelChanges && !_hasWaitingTierChanges)) {
        return RE::BSEventNotifyControl::kContinue;
    }

    // Combat between other actors never takes the lock.
    const auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || event->actor.get() != player) {
        return RE::BSEventNotifyControl::kContinue;
    }

    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    if (_pendingLevelChanges.empty() && _waitingTierChanges.empty()) {
        return RE::BSEventNotifyControl::kContinue;
    }

    if (event->newState == RE::ACTOR_COMBAT_STATE::kNone) {
        FlushPendingLevelChanges();
        ReleaseTierChanges(false);
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl RomanceManager::ProcessEvent(
    const RE::MenuOpenCloseEvent* event,
    [[maybe_unused]] RE::BSTEventSource<RE::MenuOpenCloseEvent>* eventSource)
{
    if (!event) {
        return RE::BSEventNotifyControl::kContinue;
    }
    if (!event->opening && event->menuName == RE::DialogueMenu::MENU_NAME) {
        OnDialogueClosed();
    } else if (event->opening && event->menuName == RE::SleepWaitMenu::MENU_NAME) {
        // A dismissal or recruit applied by Papyrus after dialogue closed is seen before the hours pass.
        OnDialogueClosed();
    }
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl RomanceManager::ProcessEvent(
    const RE::TESActorLocationChangeEvent* event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESActorLocationChangeEvent>* eventSource)
{
    // Every actor's moves arrive here; only the player's matter, and only while a change waits.
    if (!event || !_hasWaitingTierChanges) {
        return RE::BSEventNotifyControl::kContinue;
    }
    const auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || event->actor.get() != player) {
        return RE::BSEventNotifyControl::kContinue;
    }

    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    ReleaseTierChanges(false, std::optional<const RE::BGSLocation*>(event->newLoc));
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl RomanceManager::ProcessEvent(
    const RE::TESSleepStopEvent* event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESSleepStopEvent>* eventSource)
{
    if (!event || !_hasWaitingTierChanges) {
        return RE::BSEventNotifyControl::kContinue;
    }

    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    // An interrupted rest (an attack, a guard) is not rest; it is still a look at the place.
    ReleaseTierChanges(!event->interrupted);
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl RomanceManager::ProcessEvent(
    const RE::TESWaitStopEvent* event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESWaitStopEvent>* eventSource)
{
    if (!event || !_hasWaitingTierChanges) {
        return RE::BSEventNotifyControl::kContinue;
    }

    romantasy::OutboxLock::Scope lock(_lock);
    _viewDirty = true;
    // An interrupted rest (an attack, a guard) is not rest; it is still a look at the place.
    ReleaseTierChanges(!event->interrupted);
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
