#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

// How often conversation may move a bond. Pure: the caller keeps one state per
// follower, in memory only. Touched (gain) and hurt (loss) each have their own
// hour cooldown and their own day cap. The day is floor(gameDay).
namespace romantasy::skyrimnet
{
    struct ConversationState
    {
        float lastTouched = -1.0f, lastHurt = -1.0f;
        std::int32_t day = -1;
        std::int32_t gained = 0, lost = 0;
    };

    inline constexpr std::int32_t kConversationPoints = 5, kConversationDayCap = 15;
    inline constexpr float kConversationCooldownDays = 1.0f / 24.0f;

    namespace detail
    {
        [[nodiscard]] inline bool ValidDay(float gameDay)
        {
            return std::isfinite(gameDay) && gameDay >= 0.0f;
        }

        [[nodiscard]] inline std::int32_t DayOf(float gameDay)
        {
            const double d = std::floor(static_cast<double>(gameDay));
            if (d >= static_cast<double>(std::numeric_limits<std::int32_t>::max())) return std::numeric_limits<std::int32_t>::max();
            return static_cast<std::int32_t>(d);
        }

        // The state as it stands on gameDay. A clock that went backwards (an
        // earlier day, or earlier than a stored time) starts the state fresh
        // instead of locking it; a later day keeps the cooldown times and clears
        // the day counts.
        [[nodiscard]] inline ConversationState Normalised(ConversationState state, float gameDay)
        {
            const std::int32_t day = DayOf(gameDay);
            if ((state.day >= 0 && day < state.day) || (state.lastTouched >= 0.0f && gameDay < state.lastTouched) ||
                (state.lastHurt >= 0.0f && gameDay < state.lastHurt)) {
                state = ConversationState{};
            }
            if (state.day != day) {
                state.day = day;
                state.gained = 0;
                state.lost = 0;
            }
            return state;
        }
    }

    [[nodiscard]] inline bool CanNudge(const ConversationState& state, bool touched, float gameDay)
    {
        if (!detail::ValidDay(gameDay)) return false;
        const ConversationState now = detail::Normalised(state, gameDay);
        const float last = touched ? now.lastTouched : now.lastHurt;
        // A small tolerance (under 9 in-game seconds): float rounding must not turn "exactly an hour" into "too soon".
        if (last >= 0.0f && static_cast<double>(gameDay) - static_cast<double>(last) < static_cast<double>(kConversationCooldownDays) - 1e-4) return false;
        const std::int64_t total = static_cast<std::int64_t>(touched ? now.gained : now.lost) + kConversationPoints;
        return total <= kConversationDayCap;
    }

    // Call after a nudge was attempted: applied, or held at the gate.
    inline void Noted(ConversationState& state, bool touched, float gameDay)
    {
        if (!detail::ValidDay(gameDay)) return;
        state = detail::Normalised(state, gameDay);
        std::int32_t& count = touched ? state.gained : state.lost;
        const std::int64_t total = static_cast<std::int64_t>(count) + kConversationPoints;
        count = total > std::numeric_limits<std::int32_t>::max() ? std::numeric_limits<std::int32_t>::max() : static_cast<std::int32_t>(total);
        (touched ? state.lastTouched : state.lastHurt) = gameDay;
    }
}
