#pragma once

#include "ui/screens/UiState.h"

#include <algorithm>
#include <cstdint>

namespace romantasy::ui::tiers
{
    inline constexpr int Count = 6;
    inline constexpr std::int32_t Thresholds[Count] = { 0, 500, 1000, 1500, 2000, 2500 };
    inline constexpr const char* Names[Count] = { "Stranger", "Acquaintance", "Friend", "Confidant", "Lover", "Spouse" };
    inline constexpr std::int32_t Ladder = Thresholds[Count - 1];

    [[nodiscard]] constexpr int TierIndex(std::int32_t points) noexcept
    {
        int index = 0;
        for (int i = 1; i < Count; ++i) {
            if (points >= Thresholds[i]) {
                index = i;
            }
        }
        return index;
    }

    // -1 once the ladder is complete (Spouse).
    [[nodiscard]] constexpr std::int32_t NextThreshold(std::int32_t points) noexcept
    {
        const int index = TierIndex(points);
        return index + 1 < Count ? Thresholds[index + 1] : -1;
    }

    [[nodiscard]] constexpr std::int32_t UntilNext(std::int32_t points) noexcept
    {
        const std::int32_t next = NextThreshold(points);
        return next < 0 ? 0 : next - points;
    }

    // Fill fraction of the whole ladder (0 at Stranger, 1 at Spouse).
    [[nodiscard]] constexpr float MeterFraction(std::int32_t points) noexcept
    {
        if (points <= 0) return 0.0f;
        if (points >= Ladder) return 1.0f;
        return static_cast<float>(points) / static_cast<float>(Ladder);
    }

    // Progress within the current 500-point band; 1.0 at Spouse.
    [[nodiscard]] constexpr float TierProgress(std::int32_t points) noexcept
    {
        if (points <= 0) return 0.0f;
        if (points >= Ladder) return 1.0f;
        const std::int32_t lower = (points / 500) * 500;
        return static_cast<float>(points - lower) / 500.0f;
    }

    struct Counts
    {
        int bonds = 0;
        int following = 0;
    };

    [[nodiscard]] inline Counts CountBonds(const UiState& state, bool includeAway) noexcept
    {
        Counts counts;
        for (const auto& follower : state.followers) {
            if (!includeAway && !follower.isFollowing) continue;
            ++counts.bonds;
            if (follower.isFollowing) ++counts.following;
        }
        return counts;
    }
}
