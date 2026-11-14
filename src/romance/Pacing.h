#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

// Relationship pace and the moment a tier change is announced. Pure: no game
// headers. Nothing here depends on how often it is called.
namespace romantasy::pacing
{
    enum class Pace : std::uint8_t
    {
        Slow,
        Standard,
        Fast
    };

    enum class TierMoment : std::uint8_t
    {
        Calm,
        Immediate
    };

    constexpr std::optional<Pace> ParsePace(std::string_view key) noexcept
    {
        if (key == "slow") return Pace::Slow;
        if (key == "standard") return Pace::Standard;
        if (key == "fast") return Pace::Fast;
        return std::nullopt;
    }

    constexpr std::string_view Key(Pace pace) noexcept
    {
        switch (pace) {
        case Pace::Slow: return "slow";
        case Pace::Fast: return "fast";
        default: return "standard";
        }
    }

    constexpr std::optional<TierMoment> ParseTierMoment(std::string_view key) noexcept
    {
        if (key == "calm") return TierMoment::Calm;
        if (key == "immediate") return TierMoment::Immediate;
        return std::nullopt;
    }

    constexpr std::string_view Key(TierMoment moment) noexcept
    {
        return moment == TierMoment::Immediate ? "immediate" : "calm";
    }

    namespace detail
    {
        inline constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
        inline constexpr std::int32_t kTierSize = 500;
        inline constexpr std::int32_t kTopLevel = 6;
        inline constexpr float kWaitLimitDays = 1.0f;

        constexpr bool Finite(float value) noexcept
        {
            return value - value == 0.0f;
        }

        // Non-finite or negative days count as no days.
        constexpr float SafeDays(float days) noexcept
        {
            return Finite(days) && days > 0.0f ? days : 0.0f;
        }

        constexpr std::int32_t Narrow(std::int64_t value) noexcept
        {
            if (value > std::numeric_limits<std::int32_t>::max()) return std::numeric_limits<std::int32_t>::max();
            if (value < std::numeric_limits<std::int32_t>::min()) return std::numeric_limits<std::int32_t>::min();
            return static_cast<std::int32_t>(value);
        }

        // Stranger 1, Acquaintance 2 ... Spouse 6; a tier starts every 500 points.
        constexpr std::int32_t LevelOf(std::int32_t points) noexcept
        {
            if (points < 0) return 1;
            const std::int32_t level = points / kTierSize + 1;
            return level > kTopLevel ? kTopLevel : level;
        }
    }

    struct Paced
    {
        std::int32_t points = 0;
        bool carry = false;
    };

    // Only gains are paced. Fast doubles; Slow halves, carrying the odd half
    // point to the next gain. Losses and Standard pass through untouched.
    constexpr Paced ApplyPace(Pace pace, std::int32_t points, bool carry) noexcept
    {
        if (points <= 0) {
            return { points, carry };
        }
        const std::int64_t wide = points;
        switch (pace) {
        case Pace::Fast:
            return { detail::Narrow(wide * 2), carry };
        case Pace::Slow: {
            const std::int64_t total = wide + (carry ? 1 : 0);
            return { detail::Narrow(total / 2), total % 2 != 0 };
        }
        default:
            return { points, carry };
        }
    }

    // Days together needed to reach `level`. Stranger and Acquaintance have none.
    constexpr float DaysNeeded(std::int32_t level) noexcept
    {
        switch (level) {
        case 3: return 3.0f;
        case 4: return 7.0f;
        case 5: return 14.0f;
        case 6: return 21.0f;
        default: return 0.0f;
        }
    }

    // Highest points reachable with this many days together.
    constexpr std::int32_t GateCap(float daysTogether) noexcept
    {
        const float days = detail::SafeDays(daysTogether);
        if (days < 3.0f) return 999;
        if (days < 7.0f) return 1499;
        if (days < 14.0f) return 1999;
        if (days < 21.0f) return 2499;
        return detail::kMax;
    }

    // Highest points inside the tier `points` is in; the top tier has no top.
    constexpr std::int32_t TierTop(std::int32_t points) noexcept
    {
        const std::int32_t level = detail::LevelOf(points);
        return level >= detail::kTopLevel ? detail::kMax : level * detail::kTierSize - 1;
    }

    // A gain stops at the gate but never pulls points down, and never stops inside the
    // tier the bond already holds: a gate only keeps it from entering a higher tier early.
    // A loss or no change is returned as is.
    constexpr std::int32_t GatedPoints(std::int32_t oldPoints, std::int32_t newPoints, float daysTogether) noexcept
    {
        if (newPoints <= oldPoints) {
            return newPoints;
        }
        const std::int32_t gate = GateCap(daysTogether);
        const std::int32_t tierTop = TierTop(oldPoints);
        const std::int32_t cap = gate > tierTop ? gate : tierTop;
        const std::int32_t held = newPoints < cap ? newPoints : cap;
        return oldPoints > held ? oldPoints : held;
    }

    struct GateResult
    {
        std::int32_t newPoints = 0;
        std::int32_t applied = 0;  // newPoints - oldPoints
        bool refused = false;      // a positive delta the gate cut to 0
    };

    // The points after `delta`, before the clamp at 0 (the manager keeps that). Only a Slow
    // burn gain on a gated path is gated; a loss, Standard, Fast and the developer tools
    // (gated == false) pass through. The sum saturates at the int32 limits.
    constexpr GateResult GateGain(std::int32_t oldPoints, std::int32_t delta, float daysTogether, Pace pace, bool gated) noexcept
    {
        const std::int32_t target = detail::Narrow(static_cast<std::int64_t>(oldPoints) + delta);
        std::int32_t points = target;
        bool refused = false;
        if (gated && pace == Pace::Slow && delta > 0) {
            points = GatedPoints(oldPoints, target, daysTogether);
            refused = points == oldPoints;
        }
        return { points, detail::Narrow(static_cast<std::int64_t>(points) - oldPoints), refused };
    }

    struct Gate
    {
        std::int32_t level = 0;
        float daysNeeded = 0.0f;
    };

    // The next tier above `points`, when it still needs more days together.
    constexpr std::optional<Gate> NextGate(std::int32_t points, float daysTogether) noexcept
    {
        const std::int32_t current = detail::LevelOf(points);
        if (current >= detail::kTopLevel) {
            return std::nullopt;
        }
        const std::int32_t next = current + 1;
        const float needed = DaysNeeded(next);
        if (needed > detail::SafeDays(daysTogether)) {
            return Gate{ next, needed };
        }
        return std::nullopt;
    }

    struct CalmFacts
    {
        bool inCombat = false;
        bool calmPlace = false;
        bool justRested = false;
        bool inScene = false;  // the player is in an OStim scene and the hold is on
    };

    // Skyrim.esm location keywords that make a calm place: LocTypeInn, LocTypePlayerHouse,
    // LocTypeHouse, LocTypeDwelling, LocTypeCity, LocTypeTown, LocTypeSettlement. Only the
    // player's current location is tested, not its parents.
    inline constexpr std::array<std::uint32_t, 7> kCalmPlaceKeywords{
        0x1CB87, 0xFC1A3, 0x1CB85, 0x130DC, 0x13168, 0x13166, 0x13167
    };

    constexpr bool IsCalm(CalmFacts facts) noexcept
    {
        return !facts.inCombat && !facts.inScene && (facts.calmPlace || facts.justRested);
    }

    // Immediate: out of combat. Calm: a calm moment, or a day of waiting out of combat.
    // Neither while a scene holds.
    constexpr bool ShouldAnnounce(TierMoment moment, CalmFacts facts, float waitedDays) noexcept
    {
        if (facts.inScene) {
            return false;
        }
        if (moment == TierMoment::Immediate) {
            return !facts.inCombat;
        }
        return IsCalm(facts) || (!facts.inCombat && detail::SafeDays(waitedDays) >= detail::kWaitLimitDays);
    }

    struct PendingChange
    {
        std::int32_t oldLevel = 0;
        std::int32_t newLevel = 0;
        std::int32_t pointsDelta = 0;
        float since = 0.0f;
        // A tier whisper may stand in for the popup: false when a developer tool made the change. Not saved, so a
        // change read back from a save whispers (it was a player's change, or an author's).
        bool whisper = true;

        bool operator==(const PendingChange&) const = default;
    };

    // What to announce for a waiting change, given the level the bond holds now.
    // Nothing when the bond is back at the level it started from.
    [[nodiscard]] constexpr std::optional<PendingChange> ReleaseTarget(PendingChange waiting, std::int32_t currentLevel) noexcept
    {
        if (currentLevel < 1 || currentLevel > detail::kTopLevel || currentLevel == waiting.oldLevel) {
            return std::nullopt;
        }
        waiting.newLevel = currentLevel;
        return waiting;
    }

    // Fold a new tier change into the one already waiting. The result keeps the
    // earliest old level and time; it vanishes when the bond ends where it began.
    constexpr std::optional<PendingChange> Merge(std::optional<PendingChange> waiting, PendingChange next) noexcept
    {
        PendingChange result = next;
        if (waiting) {
            result.oldLevel = waiting->oldLevel;
            result.since = waiting->since;
            result.pointsDelta = detail::Narrow(static_cast<std::int64_t>(waiting->pointsDelta) + next.pointsDelta);
            result.whisper = waiting->whisper || next.whisper;
        }
        if (result.oldLevel == result.newLevel) {
            return std::nullopt;
        }
        return result;
    }

    enum class Release : std::uint8_t
    {
        Keep,
        Drop,
        Announce
    };

    struct ReleaseFacts
    {
        bool followerKnown = false;
        bool bondEnabled = true;
        std::int32_t currentLevel = 0;
        TierMoment moment = TierMoment::Calm;
        CalmFacts calm;
        float waitedDays = 0.0f;
    };

    struct ReleaseDecision
    {
        Release what = Release::Keep;
        PendingChange change;  // meaningful only for Announce
    };

    // What to do with one waiting change at a look, in order: a follower not in memory keeps
    // it (a reference may not be loaded yet); a switched-off bond drops it unsent; a change
    // whose moment has not come keeps waiting; one back where it began is dropped; anything
    // else is announced for the level the bond holds now.
    [[nodiscard]] constexpr ReleaseDecision DecideRelease(PendingChange waiting, ReleaseFacts facts) noexcept
    {
        if (!facts.followerKnown) {
            return { Release::Keep, waiting };
        }
        if (!facts.bondEnabled) {
            return { Release::Drop, waiting };
        }
        if (!ShouldAnnounce(facts.moment, facts.calm, facts.waitedDays)) {
            return { Release::Keep, waiting };
        }
        const auto target = ReleaseTarget(waiting, facts.currentLevel);
        if (!target) {
            return { Release::Drop, waiting };
        }
        return { Release::Announce, *target };
    }

    struct StatAward
    {
        std::int32_t applied = 0;   // points added before the clamp at 0 (0 when refused or rounded away)
        std::int32_t reported = 0;  // what Romantasy_OnPreference carries; 0 means nothing is sent
        bool carry = false;         // Slow burn's half point after this award
        bool refused = false;       // a Slow burn gate held the whole gain
    };

    // The result for one follower and one statistic delta. A gate holds points, not the
    // follower's reaction: a gain refused in full still reports the paced amount the deed was
    // worth. A gain awarded in part or in full reports what was applied; a loss what was asked.
    // A refused gain keeps the carry it came in with.
    [[nodiscard]] constexpr StatAward AwardForStat(
        std::int32_t oldPoints, std::int32_t delta, Pace pace, bool carry, float daysTogether, bool gated) noexcept
    {
        const auto paced = ApplyPace(pace, delta, carry);
        if (paced.points == 0) {
            return { 0, 0, paced.carry, false };
        }
        const auto gate = GateGain(oldPoints, paced.points, daysTogether, pace, gated);
        if (gate.refused) {
            return { 0, paced.points, carry, true };
        }
        return { gate.applied, paced.points > 0 ? gate.applied : paced.points, paced.carry, false };
    }
}
