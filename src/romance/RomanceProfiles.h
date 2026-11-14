#pragma once

#include "ostim/Moments.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace romantasy
{
    inline constexpr std::size_t kMaxStrongPreferences = 3;
    inline constexpr std::size_t kMaxUnlockLines = 4;
    inline constexpr std::size_t kMaxUnlockLineLength = 60;  // Unicode code points

    struct NpcProfile
    {
        std::string plugin;
        std::uint32_t localID = 0;
        std::int32_t startingLevel = 1;
        // Weights are -2 (pet peeve), -1 (dislike), 1 (like) or 2 (favorite).
        std::unordered_map<std::string, std::int32_t> preferences;
        // Unlock lines per tier; the index is the level minus 1 (Stranger, index 0, stays empty).
        std::array<std::vector<std::string>, 6> unlocks;
        // How the follower feels about shared moments (version 3 [moments]); defaults when absent.
        romantasy::ostim::Welcome moments;
        std::string source;
        // Ownership comes from the loader's directory, never a field supplied by a mod.
        bool playerOwned = false;
        bool enabled = true;
    };

    struct ProfileLoadResult
    {
        std::vector<NpcProfile> profiles;
        std::vector<std::string> errors;
    };

    // Returns a reason when a weight is not -2, -1, 1 or 2, or when more than
    // kMaxStrongPreferences are favorites or more than that are pet peeves.
    [[nodiscard]] std::optional<std::string> CheckPreferenceWeights(const std::unordered_map<std::string, std::int32_t>& preferences);

    // Demotes favorites beyond kMaxStrongPreferences to 1 and pet peeves beyond it to -1, keeping the
    // first ones in ascending key order. Returns how many weights it demoted.
    std::size_t EnforceStrongCap(std::unordered_map<std::string, std::int32_t>& weights);

    // delta * rulePoints * weight in 64 bits, clamped to the int32 range.
    [[nodiscard]] std::int32_t PreferencePoints(std::int32_t delta, std::int32_t rulePoints, std::int32_t weight);

    // The Papyrus view of a weight: -1 (dislike or pet peeve), 0 or 1 (like or favorite).
    [[nodiscard]] std::int32_t PapyrusDirection(std::int32_t weight);

    // Parse/validate the whole profile before returning it. Invalid input throws
    // with a filename and actionable error. No Skyrim forms are accessed here.
    [[nodiscard]] NpcProfile ParseProfile(std::string_view text, std::string_view source, bool playerOwned = false);
    [[nodiscard]] ProfileLoadResult LoadProfiles(const std::filesystem::path& directory, bool playerOwned = false);
    // Validates and replaces a complete player profile atomically, or throws.
    // Disabled profiles are retained to prevent older saves/overlays reviving a bond.
    [[nodiscard]] NpcProfile SavePlayerProfile(const std::filesystem::path& directory, const NpcProfile& profile);
}
