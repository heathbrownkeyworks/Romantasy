#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace romantasy
{
    struct NpcProfile
    {
        std::string plugin;
        std::uint32_t localID = 0;
        std::int32_t startingLevel = 1;
        std::unordered_map<std::string, std::int32_t> preferences;
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

    // Parse/validate the whole profile before returning it. Invalid input throws
    // with a filename and actionable error. No Skyrim forms are accessed here.
    [[nodiscard]] NpcProfile ParseProfile(std::string_view text, std::string_view source, bool playerOwned = false);
    [[nodiscard]] ProfileLoadResult LoadProfiles(const std::filesystem::path& directory, bool playerOwned = false);
    // Validates and replaces a complete player profile atomically, or throws.
    // Disabled profiles are retained to prevent older saves/overlays reviving a bond.
    [[nodiscard]] NpcProfile SavePlayerProfile(const std::filesystem::path& directory, const NpcProfile& profile);
}
