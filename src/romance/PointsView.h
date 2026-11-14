#pragma once

#include "romance/Mood.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace romantasy
{
    // An immutable snapshot of what romance conditions and point getters read. The manager
    // rebuilds it under its lock and publishes it before the lock is released, so readers
    // answer from one consistent copy without taking the lock.
    struct PointsView
    {
        struct Profile
        {
            bool enabled = true;
            std::int32_t startingPoints = 0;
        };

        std::unordered_map<std::uint32_t, std::int32_t> byReference;  // follower reference FormID -> points (never key 0)
        std::unordered_map<std::uint32_t, Profile> profiles;          // base NPC FormID -> profile
        std::unordered_map<std::uint32_t, std::int32_t> saved;        // copy of the saved-points table
        std::unordered_map<std::uint32_t, mood::Mood> moodByReference;  // follower reference FormID -> mood (never key 0)

        [[nodiscard]] std::optional<std::int32_t> Find(std::uint32_t reference) const;
        // The stored mood for a known reference; the reader drifts it to the current day.
        [[nodiscard]] std::optional<mood::Mood> FindMood(std::uint32_t reference) const;
        // Today's RomanceManager::QueryPoints: a disabled profile answers nothing, a known
        // reference answers its points, a profile answers saved or starting points.
        [[nodiscard]] std::optional<std::int32_t> Query(std::uint32_t reference, std::uint32_t base, bool baseIsUnique) const;
    };
}
