#include "romance/RomanceProfiles.h"
#include "romance/RomanceStats.h"
#include "util/AtomicFile.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <format>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <sstream>
#include <system_error>
#include <unordered_set>

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
            const auto version = doc["schema_version"].is_integer() ? doc["schema_version"].value<std::int64_t>().value_or(0) : 0;
            // Version 3 is version 2 plus [moments].
            const bool v3 = version == 3;
            const bool v2 = version == 2 || v3;
            if (v3) CheckKeys(doc, { "schema_version", "npc", "romance", "unlocks", "moments" });
            else if (v2) CheckKeys(doc, { "schema_version", "npc", "romance", "unlocks" });
            else CheckKeys(doc, { "schema_version", "npc", "romance" });
            if (!v2 && version != 1) {
                throw std::runtime_error("schema_version must be integer 1, 2 or 3");
            }
            const auto* npc = doc["npc"].as_table();
            const auto* romance = doc["romance"].as_table();
            if (!npc || !romance) throw std::runtime_error("[npc] and [romance] tables are required");
            CheckKeys(*npc, { "plugin", "form_id" });
            if (v2 && playerOwned) CheckKeys(*romance, { "starting_level", "likes", "dislikes", "favorites", "pet_peeves", "enabled" });
            else if (v2) CheckKeys(*romance, { "starting_level", "likes", "dislikes", "favorites", "pet_peeves" });
            else if (playerOwned) CheckKeys(*romance, { "starting_level", "likes", "dislikes", "enabled" });
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
            if (v2) {
                readOpinions("favorites", 2);
                readOpinions("pet_peeves", -2);
                const auto count = [&](std::int32_t weight) {
                    return static_cast<std::size_t>(std::ranges::count_if(profile.preferences,
                        [&](const auto& entry) { return entry.second == weight; }));
                };
                if (count(2) > kMaxStrongPreferences) throw std::runtime_error("favorites lists more than 3 statistics");
                if (count(-2) > kMaxStrongPreferences) throw std::runtime_error("pet_peeves lists more than 3 statistics");

                if (doc.contains("unlocks")) {
                    if (playerOwned) throw std::runtime_error("[unlocks] is not allowed in a player profile");
                    const auto* unlocks = doc["unlocks"].as_table();
                    if (!unlocks) throw std::runtime_error("[unlocks] must be a table");
                    for (const auto& [key, value] : *unlocks) {
                        const std::string tier(key.str());
                        if (tier == "Stranger") throw std::runtime_error("[unlocks] cannot name Stranger");
                        const auto found = std::find(tiers.begin(), tiers.end(), tier);
                        if (found == tiers.end()) throw std::runtime_error("unknown tier '" + tier + "' in [unlocks]");
                        const auto* lines = value.as_array();
                        if (!lines || lines->empty() || lines->size() > kMaxUnlockLines) {
                            throw std::runtime_error("unlocks for " + tier + " must be a list of 1 to 4 lines");
                        }
                        auto& out = profile.unlocks[static_cast<std::size_t>(found - tiers.begin())];
                        for (const auto& item : *lines) {
                            const auto line = item.value<std::string>();
                            if (!line) throw std::runtime_error("unlocks for " + tier + " must be a list of 1 to 4 lines");
                            std::size_t length = 0;
                            bool clean = true;
                            for (const unsigned char c : *line) {
                                if ((c & 0xC0) != 0x80) ++length;
                                if (c < 0x20 || c == 0x7F) clean = false;
                            }
                            if (length == 0 || length > kMaxUnlockLineLength || !clean) {
                                throw std::runtime_error("an unlock line for " + tier + " must be 1 to 60 characters with no control characters");
                            }
                            out.push_back(*line);
                        }
                    }
                }
            }

            if (v3 && doc.contains("moments")) {
                if (playerOwned) throw std::runtime_error("[moments] is not allowed in a player profile");
                const auto* moments = doc["moments"].as_table();
                if (!moments) throw std::runtime_error("[moments] must be a table");
                CheckKeys(*moments, { "affection", "closeness", "intimacy", "feeling" });
                const auto readTier = [&](std::string_view key, std::int32_t& out) {
                    const auto* node = moments->get(key);
                    if (!node) return;
                    const auto text = node->is_string() ? node->value<std::string>() : std::nullopt;
                    if (text == "never") {
                        out = 0;
                        return;
                    }
                    const auto found = text ? std::find(tiers.begin(), tiers.end(), *text) : tiers.end();
                    if (found == tiers.end()) {
                        std::ostringstream shown;
                        if (text) shown << *text;
                        else node->visit([&](const auto& value) { shown << value; });
                        throw std::runtime_error("unknown tier '" + shown.str() + "' in [moments]");
                    }
                    out = static_cast<std::int32_t>(found - tiers.begin()) + 1;
                };
                readTier("affection", profile.moments.affection);
                readTier("closeness", profile.moments.closeness);
                readTier("intimacy", profile.moments.intimacy);
                if (const auto* node = moments->get("feeling")) {
                    const auto feeling = node->is_string() ? node->value<std::string>() : std::nullopt;
                    if (feeling == "warm") profile.moments.feeling = romantasy::ostim::Feeling::Warm;
                    else if (feeling == "treasured") profile.moments.feeling = romantasy::ostim::Feeling::Treasured;
                    else if (feeling == "reserved") profile.moments.feeling = romantasy::ostim::Feeling::Reserved;
                    else throw std::runtime_error("feeling in [moments] must be warm, treasured or reserved");
                }
            }
            return profile;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string(source) + ": " + e.what());
        }
    }

    std::optional<std::string> CheckPreferenceWeights(const std::unordered_map<std::string, std::int32_t>& preferences)
    {
        std::size_t favorites = 0, petPeeves = 0;
        for (const auto& [name, weight] : preferences) {
            if (weight != -2 && weight != -1 && weight != 1 && weight != 2) {
                return "invalid preference weight for '" + name + "'";
            }
            if (weight == 2) ++favorites;
            if (weight == -2) ++petPeeves;
        }
        if (favorites > kMaxStrongPreferences) return "favorites lists more than 3 statistics";
        if (petPeeves > kMaxStrongPreferences) return "pet_peeves lists more than 3 statistics";
        return std::nullopt;
    }

    std::size_t EnforceStrongCap(std::unordered_map<std::string, std::int32_t>& weights)
    {
        std::vector<std::string> keys;
        keys.reserve(weights.size());
        for (const auto& entry : weights) keys.push_back(entry.first);
        std::ranges::sort(keys);
        std::size_t favorites = 0, petPeeves = 0, demoted = 0;
        for (const auto& key : keys) {
            auto& weight = weights.at(key);
            if (weight == 2 && ++favorites > kMaxStrongPreferences) {
                weight = 1;
                ++demoted;
            } else if (weight == -2 && ++petPeeves > kMaxStrongPreferences) {
                weight = -1;
                ++demoted;
            }
        }
        return demoted;
    }

    std::int32_t PreferencePoints(std::int32_t delta, std::int32_t rulePoints, std::int32_t weight)
    {
        constexpr std::int64_t lo = std::numeric_limits<std::int32_t>::min();
        constexpr std::int64_t hi = std::numeric_limits<std::int32_t>::max();
        // Clamping the first product keeps the second inside 64 bits and cannot change a clamped result,
        // because a weight that is not 0 never shrinks a value that is already out of range.
        const auto first = std::clamp(static_cast<std::int64_t>(delta) * rulePoints, lo, hi);
        return static_cast<std::int32_t>(std::clamp(first * weight, lo, hi));
    }

    std::int32_t PapyrusDirection(std::int32_t weight)
    {
        return weight < 0 ? -1 : weight > 0 ? 1 : 0;
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
        if (const auto reason = CheckPreferenceWeights(profile.preferences)) throw std::runtime_error(*reason);
        if (std::ranges::any_of(profile.unlocks, [](const auto& lines) { return !lines.empty(); })) {
            throw std::runtime_error("player profiles cannot carry unlocks");
        }
        toml::array likes, dislikes, favorites, petPeeves;
        // Sort to produce stable, readable files independent of unordered_map order.
        std::vector<std::pair<std::string, std::int32_t>> preferences(profile.preferences.begin(), profile.preferences.end());
        std::sort(preferences.begin(), preferences.end());
        for (const auto& [name, direction] : preferences) {
            (direction == 2 ? favorites : direction == -2 ? petPeeves : direction < 0 ? dislikes : likes).push_back(name);
        }
        const bool v2 = !favorites.empty() || !petPeeves.empty();
        toml::table romance{{"starting_level", tiers[profile.startingLevel - 1]},
            {"enabled", profile.enabled}, {"likes", likes}, {"dislikes", dislikes}};
        if (v2) {
            romance.insert("favorites", favorites);
            romance.insert("pet_peeves", petPeeves);
        }
        const toml::table doc{
            { "schema_version", v2 ? 2 : 1 },
            { "npc", toml::table{{"plugin", profile.plugin}, {"form_id", std::format("0x{:06X}", profile.localID)}} },
            { "romance", romance }
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
        if (!files::WriteTextAtomically(destination, text)) {
            throw std::runtime_error("cannot replace player profile");
        }
        return validated;
    }
}
