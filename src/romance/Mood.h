#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

// Short-term mood of a bond, and the company rules that decide when two
// people have been apart long enough to say so. Pure: no game headers.
namespace romantasy::mood
{
    enum class State : std::uint8_t
    {
        Strained = 1,
        Settled = 2,
        Warm = 3
    };

    struct Mood
    {
        std::int16_t score = 0;
        float at = 0.0f;  // game days when the score was set

        bool operator==(const Mood&) const = default;
    };

    inline constexpr std::int32_t kReunionNudge = 15;
    inline constexpr std::int32_t kTierLostNudge = -20;
    inline constexpr float kTimeApartDays = 3.0f;
    inline constexpr float kReunionDays = 1.0f;

    namespace detail
    {
        inline constexpr std::int32_t kDriftPerDay = 25;
        inline constexpr std::int32_t kGainPerPoint = 4;
        inline constexpr std::int32_t kGainCap = 20;
        inline constexpr std::int32_t kLossPerPoint = 6;
        inline constexpr std::int32_t kLossCap = 30;
        inline constexpr std::int32_t kLimit = 100;
        inline constexpr std::int32_t kThreshold = 25;

        constexpr std::int16_t Clamped(std::int32_t value) noexcept
        {
            return static_cast<std::int16_t>(std::clamp(value, -kLimit, kLimit));
        }
    }

    namespace detail
    {
        constexpr bool Finite(float value) noexcept
        {
            return value - value == 0.0f;
        }

        // One drift step. The stored time advances only by the drift actually
        // applied, so the fraction of a point is kept for the next call and
        // the result does not depend on how often this runs.
        constexpr Mood DriftStep(Mood mood, float now) noexcept
        {
            if (mood.score == 0) {
                return { 0, now };
            }
            const float elapsed = now - mood.at;
            if (!(elapsed > 0.0f) || !Finite(elapsed)) {
                return mood;
            }
            const float raw = elapsed * static_cast<float>(kDriftPerDay);
            const std::int32_t whole = raw >= 1000.0f ? 1000 : static_cast<std::int32_t>(raw);
            const std::int32_t size = mood.score > 0 ? mood.score : -mood.score;
            const std::int32_t applied = whole < size ? whole : size;
            const std::int32_t score = mood.score > 0 ? mood.score - applied : mood.score + applied;
            if (score == 0) {
                return { 0, now };
            }
            const float at = mood.at + static_cast<float>(applied) / static_cast<float>(kDriftPerDay);
            return { static_cast<std::int16_t>(score), at < now ? at : now };
        }
    }

    // Score moved toward 0 by 25 per day since `at`, never past 0. The drift
    // is truncated to whole points. An earlier `now` counts as no time.
    constexpr std::int16_t Drifted(Mood mood, float now) noexcept
    {
        return detail::DriftStep(mood, now).score;
    }

    constexpr State StateOf(std::int16_t score) noexcept
    {
        if (score >= detail::kThreshold) {
            return State::Warm;
        }
        if (score <= -detail::kThreshold) {
            return State::Strained;
        }
        return State::Settled;
    }

    constexpr State StateAt(Mood mood, float now) noexcept
    {
        return StateOf(Drifted(mood, now));
    }

    constexpr std::string_view Name(State state) noexcept
    {
        switch (state) {
        case State::Strained: return "Strained";
        case State::Warm: return "Warm";
        default: return "Settled";
        }
    }

    constexpr std::string_view Key(State state) noexcept
    {
        switch (state) {
        case State::Strained: return "strained";
        case State::Warm: return "warm";
        default: return "settled";
        }
    }

    // Drift, then +4 per point (cap +20) or -6 per point (cap -30), clamp.
    // A non-finite `now` changes nothing. The stamp moves only with the drift.
    constexpr Mood AfterPoints(Mood mood, std::int32_t pointsDelta, float now) noexcept
    {
        std::int32_t change = 0;
        if (pointsDelta > 0) {
            change = pointsDelta > detail::kGainCap / detail::kGainPerPoint
                ? detail::kGainCap
                : pointsDelta * detail::kGainPerPoint;
        } else if (pointsDelta < 0) {
            change = pointsDelta < -(detail::kLossCap / detail::kLossPerPoint)
                ? -detail::kLossCap
                : pointsDelta * detail::kLossPerPoint;
        }
        if (!detail::Finite(now)) {
            return mood;
        }
        const Mood drifted = detail::DriftStep(mood, now);
        return { detail::Clamped(drifted.score + change), drifted.at };
    }

    // Drift, add `amount`, clamp. Same stamp and non-finite rules as AfterPoints.
    constexpr Mood Nudged(Mood mood, std::int32_t amount, float now) noexcept
    {
        const std::int32_t bounded = std::clamp(amount, -detail::kLimit, detail::kLimit);
        if (!detail::Finite(now)) {
            return mood;
        }
        const Mood drifted = detail::DriftStep(mood, now);
        return { detail::Clamped(drifted.score + bounded), drifted.at };
    }

    struct Company
    {
        bool seenFollowing = false;
        bool away = false;
        bool apartSent = false;
        float awaySince = 0.0f;
        float lastSeen = 0.0f;      // game day of the last observation while following
        float daysTogether = 0.0f;  // in-game days spent following, summed between observations

        bool operator==(const Company&) const = default;
    };

    enum class CompanyEvent : std::uint8_t
    {
        None,
        TimeApart,
        Reunion
    };

    struct CompanyStep
    {
        Company next;
        CompanyEvent event = CompanyEvent::None;
        float daysApart = 0.0f;
    };

    constexpr CompanyStep Observe(Company company, bool following, float now) noexcept
    {
        CompanyStep step{ company };
        Company& c = step.next;
        if (!detail::Finite(now)) {
            return step;
        }

        if (following) {
            if (!c.seenFollowing) {
                c.seenFollowing = true;
                c.lastSeen = now;
            } else if (c.away) {
                const float apart = std::max(0.0f, now - c.awaySince);
                step.daysApart = apart;
                if (apart >= kReunionDays) {
                    step.event = CompanyEvent::Reunion;
                }
                c.away = false;
                c.apartSent = false;
                c.lastSeen = now;
            } else {
                c.daysTogether += std::max(0.0f, now - c.lastSeen);
                c.lastSeen = now;
            }
            return step;
        }

        if (!c.seenFollowing) {
            return step;
        }
        if (!c.away) {
            c.away = true;
            c.awaySince = now;
        } else if (!c.apartSent) {
            const float apart = now - c.awaySince;
            if (apart >= kTimeApartDays) {
                step.event = CompanyEvent::TimeApart;
                step.daysApart = apart;
                c.apartSent = true;
            }
        }
        return step;
    }

    struct Reseeded
    {
        Mood mood;
        Company company;
        bool changed = false;
    };

    // Returns the entry with every stored time made sane for `today`.
    // A time is bad when it is non-finite, negative, later than today, or (for
    // lastSeen and awaySince) zero while seenFollowing is set. Bad times become today.
    // awaySince is looked at only while the entry is away. Days together that are
    // non-finite or negative become 0. A non-finite `today` returns the entry unchanged.
    [[nodiscard]] constexpr Reseeded Reseed(Mood mood, Company company, float today) noexcept
    {
        Reseeded result{ mood, company };
        if (!detail::Finite(today)) {
            return result;
        }
        const auto fix = [today, &result](float& day, bool zeroIsBad) {
            const bool bad = !detail::Finite(day) || day < 0.0f || day > today || (zeroIsBad && day == 0.0f);
            if (bad && !(day == today)) {
                day = today;
                result.changed = true;
            }
        };
        fix(result.mood.at, false);
        fix(result.company.lastSeen, result.company.seenFollowing);
        if (result.company.away) {
            fix(result.company.awaySince, result.company.seenFollowing);
        }
        if (!detail::Finite(result.company.daysTogether) || result.company.daysTogether < 0.0f) {
            result.company.daysTogether = 0.0f;
            result.changed = true;
        }
        return result;
    }
}
