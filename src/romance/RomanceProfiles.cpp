#include "romance/RomanceProfiles.h"
#include "romance/RomanceStats.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <format>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <Windows.h>

namespace romantasy
{
    namespace
    {
        std::string Lower(std::string_view value)
        {
            std::string result(value);
            for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return result;
        }

        std::string Normalize(std::string_view value)
        {
            std::string result;
            for (const unsigned char c : value) {
                if (std::isalnum(c)) result.push_back(static_cast<char>(std::tolower(c)));
            }
            return result;
        }

        void CheckKeys(const toml::table& table, std::initializer_list<std::string_view> allowed)
        {
            for (const auto& [key, value] : table) {
                if (std::find(allowed.begin(), allowed.end(), key.str()) == allowed.end()) {
                    throw std::runtime_error("unknown field '" + std::string(key.str()) + "'");
                }
            }
        }

        std::string RequiredString(const toml::table& table, std::string_view key)
        {
            auto value = table[key].value<std::string>();
            if (!value || value->empty()) throw std::runtime_error("expected non-empty string '" + std::string(key) + "'");
            return *value;
        }

        std::string IdentityKey(const NpcProfile& profile)
        {
            return Lower(profile.plugin) + "|" + std::to_string(profile.localID);
        }
    }

    NpcProfile ParseProfile(std::string_view text, std::string_view source, bool playerOwned)
    {
        try {
            const auto doc = toml::parse(text, source);
            CheckKeys(doc, { "schema_version", "npc", "romance" });
            if (!doc["schema_version"].is_integer() || doc["schema_version"].value<std::int64_t>() != 1) {
                throw std::runtime_error("schema_version must be integer 1");
            }
            const auto* npc = doc["npc"].as_table();
            const auto* romance = doc["romance"].as_table();
            if (!npc || !romance) throw std::runtime_error("[npc] and [romance] tables are required");
            CheckKeys(*npc, { "plugin", "form_id" });
            if (playerOwned) CheckKeys(*romance, { "starting_level", "likes", "dislikes", "enabled" });
            else CheckKeys(*romance, { "starting_level", "likes", "dislikes" });

            NpcProfile profile;
            profile.source = source;
            profile.playerOwned = playerOwned;
            if (romance->contains("enabled")) {
                if (!(*romance)["enabled"].is_boolean()) throw std::runtime_error("enabled must be a boolean");
                const auto enabled = (*romance)["enabled"].value<bool>();
                if (!enabled) throw std::runtime_error("enabled must be a boolean");
                profile.enabled = *enabled;
            }
            profile.plugin = RequiredString(*npc, "plugin");
            const auto plugin = Lower(profile.plugin);
            if (plugin.size() <= 4 || plugin.find_first_of("/\\:") != std::string::npos ||
                !(plugin.ends_with(".esp") || plugin.ends_with(".esm") || plugin.ends_with(".esl"))) {
                throw std::runtime_error("plugin must be an ESP/ESM/ESL filename without a path");
            }
            const auto formID = RequiredString(*npc, "form_id");
            std::string_view hex(formID);
            if (hex.starts_with("0x") || hex.starts_with("0X")) hex.remove_prefix(2);
            const auto parsed = std::from_chars(hex.data(), hex.data() + hex.size(), profile.localID, 16);
            if (hex.empty() || hex.size() > 6 || parsed.ec != std::errc{} ||
                parsed.ptr != hex.data() + hex.size() || profile.localID == 0 || profile.localID > 0xFFFFFF) {
                throw std::runtime_error("form_id must be a non-zero plugin-local hexadecimal NPC base ID (at most 6 digits)");
            }

            constexpr std::array<std::string_view, 6> tiers = {
                "Stranger", "Acquaintance", "Friend", "Confidant", "Lover", "Spouse"
            };
            if (romance->contains("starting_level")) {
                const auto tier = RequiredString(*romance, "starting_level");
                const auto found = std::find(tiers.begin(), tiers.end(), tier);
                if (found == tiers.end()) throw std::runtime_error("starting_level must name one of the six relationship tiers");
                profile.startingLevel = static_cast<std::int32_t>(found - tiers.begin()) + 1;
            }

            const auto readOpinions = [&](std::string_view field, std::int32_t direction) {
                if (!romance->contains(field)) return;
                const auto* values = (*romance)[field].as_array();
                if (!values) throw std::runtime_error(std::string(field) + " must be an array of statistic names");
                for (const auto& value : *values) {
                    const auto name = value.value<std::string>();
                    if (!name) throw std::runtime_error(std::string(field) + " must contain only strings");
                    const auto normalized = Normalize(*name);
                    const auto rule = std::find_if(StatRules.begin(), StatRules.end(), [&](const auto& stat) {
                        return Normalize(stat.editorID) == normalized || Normalize(stat.label) == normalized;
                    });
                    if (rule == StatRules.end()) throw std::runtime_error("unknown statistic '" + *name + "'");
                    if (!profile.preferences.emplace(std::string(rule->editorID), direction).second) {
                        throw std::runtime_error("duplicate or conflicting preference for '" + std::string(rule->label) + "'");
                    }
                }
            };
            readOpinions("likes", 1);
            readOpinions("dislikes", -1);
            return profile;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string(source) + ": " + e.what());
        }
    }

    ProfileLoadResult LoadProfiles(const std::filesystem::path& directory, bool playerOwned)
    {
        ProfileLoadResult result;
        std::vector<std::filesystem::path> files;
        try {
            if (!std::filesystem::exists(directory)) return result;
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file() && Lower(entry.path().extension().string()) == ".toml") {
                    files.push_back(entry.path());
                }
            }
        } catch (const std::exception& e) {
            result.errors.push_back(directory.string() + ": " + e.what());
            return result;
        }
        std::sort(files.begin(), files.end());
        std::vector<NpcProfile> parsed;
        std::unordered_map<std::string, std::size_t> counts;
        for (const auto& path : files) {
            try {
                std::ifstream input(path, std::ios::binary);
                if (!input) throw std::runtime_error(path.string() + ": cannot open profile");
                const std::string text{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
                if (input.bad()) throw std::runtime_error(path.string() + ": failed to read profile");
                auto profile = ParseProfile(text, path.string(), playerOwned);
                ++counts[IdentityKey(profile)];
                parsed.push_back(std::move(profile));
            } catch (const std::exception& e) {
                result.errors.emplace_back(e.what());
            }
        }
        std::unordered_set<std::string> reported;
        for (auto& profile : parsed) {
            const auto key = IdentityKey(profile);
            if (counts.at(key) != 1) {
                if (reported.insert(key).second) {
                    std::string sources;
                    for (const auto& other : parsed) {
                        if (IdentityKey(other) == key) sources += " " + other.source;
                    }
                    result.errors.push_back("conflicting profiles for " + key + "; all definitions rejected:" + sources);
                }
            } else {
                result.profiles.push_back(std::move(profile));
            }
        }
        return result;
    }

    NpcProfile SavePlayerProfile(const std::filesystem::path& directory, const NpcProfile& profile)
    {
        if (!profile.playerOwned) throw std::runtime_error("cannot write a mod-owned profile");
        if (profile.plugin.find_first_of("<>:\"/\\|?*") != std::string::npos ||
            std::ranges::any_of(profile.plugin, [](unsigned char c) { return c < 32; })) {
            throw std::runtime_error("plugin is not a safe filename");
        }
        constexpr std::array<const char*, 6> tiers = {
            "Stranger", "Acquaintance", "Friend", "Confidant", "Lover", "Spouse"
        };
        if (profile.startingLevel < 1 || profile.startingLevel > 6) throw std::runtime_error("invalid starting level");
        toml::array likes, dislikes;
        // Sort to produce stable, readable files independent of unordered_map order.
        std::vector<std::pair<std::string, std::int32_t>> preferences(profile.preferences.begin(), profile.preferences.end());
        std::sort(preferences.begin(), preferences.end());
        for (const auto& [name, direction] : preferences) {
            if (direction != -1 && direction != 1) throw std::runtime_error("invalid preference direction");
            (direction < 0 ? dislikes : likes).push_back(name);
        }
        const toml::table doc{
            { "schema_version", 1 },
            { "npc", toml::table{{"plugin", profile.plugin}, {"form_id", std::format("0x{:06X}", profile.localID)}} },
            { "romance", toml::table{{"starting_level", tiers[profile.startingLevel - 1]},
                {"enabled", profile.enabled}, {"likes", likes}, {"dislikes", dislikes}} }
        };
        std::ostringstream output;
        output << toml::toml_formatter{doc} << '\n';
        const auto text = output.str();
        const auto canonical = directory / std::format("{}.{:06X}.toml", Lower(profile.plugin), profile.localID);
        // Existing loaded filenames are preserved, but can never redirect a write
        // outside the player directory (or to a different NPC's profile).
        const auto destination = profile.source.empty() ? canonical : std::filesystem::path(profile.source);
        if (destination.parent_path().lexically_normal() != directory.lexically_normal() ||
            Lower(destination.extension().string()) != ".toml") {
            throw std::runtime_error("player profile destination is outside its directory");
        }
        auto validated = ParseProfile(text, destination.string(), true);
        if (std::filesystem::exists(destination)) {
            std::ifstream input(destination, std::ios::binary);
            if (!input) throw std::runtime_error("cannot read existing player profile");
            const std::string previous{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
            if (input.bad() || IdentityKey(ParseProfile(previous, destination.string(), true)) != IdentityKey(validated)) {
                throw std::runtime_error("existing player profile belongs to another NPC or could not be read");
            }
        }
        std::filesystem::create_directories(directory);
        const std::filesystem::path temporary = destination.string() + ".tmp";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (!file) throw std::runtime_error("cannot create player profile temporary file");
            file << text;
            file.close();
            if (!file) throw std::runtime_error("failed to write complete player profile");
        }
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "cannot replace player profile");
        }
        return validated;
    }
}
