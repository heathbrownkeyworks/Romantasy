#include "profiles/Lint.h"

#include "romance/RomanceProfiles.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <format>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

namespace romantasy::profilecheck
{
    namespace
    {
        constexpr std::size_t kPopupLineWidth = 48;
        constexpr std::array<std::string_view, 5> kUnlockTiers = { "Acquaintance", "Friend", "Confidant", "Lover", "Spouse" };
        constexpr std::array<std::string_view, 6> kTiers = { "Stranger", "Acquaintance", "Friend", "Confidant", "Lover", "Spouse" };

        // The keys ParseProfile accepts, in the order it lists them. Keep in step with RomanceProfiles.cpp;
        // a test feeds a file holding every key listed here to the real parser.
        std::vector<std::string_view> DocKeys(bool v2, bool v3)
        {
            if (v3) return { "schema_version", "npc", "romance", "unlocks", "moments" };
            if (v2) return { "schema_version", "npc", "romance", "unlocks" };
            return { "schema_version", "npc", "romance" };
        }

        std::vector<std::string_view> NpcKeys() { return { "plugin", "form_id" }; }

        std::vector<std::string_view> MomentKeys() { return { "affection", "closeness", "intimacy", "feeling" }; }

        std::vector<std::string_view> RomanceKeys(bool v2, bool playerOwned)
        {
            std::vector<std::string_view> keys = { "starting_level", "likes", "dislikes" };
            if (v2) {
                keys.push_back("favorites");
                keys.push_back("pet_peeves");
            }
            if (playerOwned) keys.push_back("enabled");
            return keys;
        }

        std::size_t Distance(std::string_view a, std::string_view b)
        {
            std::vector<std::size_t> previous(b.size() + 1), current(b.size() + 1);
            for (std::size_t j = 0; j <= b.size(); ++j) previous[j] = j;
            for (std::size_t i = 1; i <= a.size(); ++i) {
                current[0] = i;
                for (std::size_t j = 1; j <= b.size(); ++j) {
                    const std::size_t substitute = previous[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
                    current[j] = std::min({ previous[j] + 1, current[j - 1] + 1, substitute });
                }
                std::swap(previous, current);
            }
            return previous[b.size()];
        }

        std::int32_t LineOf(const toml::source_region& source)
        {
            return static_cast<std::int32_t>(source.begin.line);
        }

        std::string Lower(std::string_view value)
        {
            std::string result(value);
            for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return result;
        }

        std::size_t CodePoints(std::string_view text)
        {
            std::size_t count = 0;
            for (const unsigned char c : text) {
                if ((c & 0xC0) != 0x80) ++count;
            }
            return count;
        }

        void Add(std::vector<Finding>& out, Finding::Level level, std::int32_t line, std::string message)
        {
            out.push_back({ level, line, std::move(message) });
        }

        void CheckUnknownKeys(std::vector<Finding>& out, const toml::table& table, const std::vector<std::string_view>& known)
        {
            for (const auto& [key, value] : table) {
                const std::string_view name = key.str();
                if (std::find(known.begin(), known.end(), name) != known.end()) continue;
                std::string_view nearest;
                std::size_t best = 3;  // only a distance of 2 or less is suggested
                for (const auto candidate : known) {
                    const auto distance = Distance(name, candidate);
                    if (distance < best) {
                        best = distance;
                        nearest = candidate;
                    }
                }
                auto message = std::format("unknown key '{}'", name);
                if (!nearest.empty()) message += std::format(" (did you mean '{}'?)", nearest);
                Add(out, Finding::Level::Warning, LineOf(key.source()), std::move(message));
            }
        }

        void Warn(std::vector<Finding>& out, const toml::table& doc, bool playerOwned)
        {
            const auto version = doc["schema_version"].is_integer() ? doc["schema_version"].value<std::int64_t>().value_or(0) : 0;
            const bool v3 = version == 3;
            const bool v2 = version == 2 || v3;
            CheckUnknownKeys(out, doc, DocKeys(v2, v3));

            if (const auto* npc = doc["npc"].as_table()) {
                CheckUnknownKeys(out, *npc, NpcKeys());
                if (const auto* plugin = (*npc)["plugin"].as_string()) {
                    const auto lower = Lower(plugin->get());
                    if (!(lower.ends_with(".esp") || lower.ends_with(".esm") || lower.ends_with(".esl"))) {
                        Add(out, Finding::Level::Warning, LineOf(plugin->source()),
                            std::format("plugin '{}' has no .esp, .esm or .esl ending", plugin->get()));
                    }
                }
            }

            if (const auto* romance = doc["romance"].as_table()) {
                CheckUnknownKeys(out, *romance, RomanceKeys(v2, playerOwned));
                bool anyOpinion = false;
                for (const auto field : { "likes", "dislikes", "favorites", "pet_peeves" }) {
                    const auto* node = romance->get(field);
                    if (!node) continue;
                    const auto* list = node->as_array();
                    if (!list || !list->empty()) anyOpinion = true;
                }
                if (!anyOpinion) {
                    Add(out, Finding::Level::Warning, LineOf(romance->source()),
                        "no likes, dislikes, favorites or pet peeves: this follower reacts to nothing");
                }
            }

            if (const auto* unlocks = doc["unlocks"].as_table()) {
                CheckUnknownKeys(out, *unlocks, { kUnlockTiers.begin(), kUnlockTiers.end() });
                for (const auto tier : kUnlockTiers) {
                    const auto* lines = unlocks->get(tier) ? unlocks->get(tier)->as_array() : nullptr;
                    if (!lines) continue;
                    for (const auto& item : *lines) {
                        const auto* text = item.as_string();
                        if (!text) continue;
                        const auto length = CodePoints(text->get());
                        if (length > kPopupLineWidth) {
                            Add(out, Finding::Level::Warning, LineOf(item.source()),
                                std::format("unlock line for {} is {} characters and will wrap on the popup ({} fit on one line)",
                                    tier, length, kPopupLineWidth));
                        }
                    }
                }
            }

            if (const auto* moments = doc["moments"].as_table(); v3 && moments) {
                CheckUnknownKeys(out, *moments, MomentKeys());
                // The level each kind is welcome from, defaults filling a missing key; 0 for never or a bad value.
                struct Entry
                {
                    std::string_view key;
                    std::int32_t level;
                    std::int32_t line;
                };
                const romantasy::ostim::Welcome defaults;
                std::array<Entry, 3> kinds = { { { "affection", defaults.affection, 0 },
                    { "closeness", defaults.closeness, 0 },
                    { "intimacy", defaults.intimacy, 0 } } };
                for (auto& kind : kinds) {
                    const auto* node = moments->get(kind.key);
                    if (!node) continue;
                    kind.line = LineOf(node->source());
                    const auto* text = node->as_string();
                    const auto found = text ? std::find(kTiers.begin(), kTiers.end(), text->get()) : kTiers.end();
                    kind.level = found == kTiers.end() ? 0 : static_cast<std::int32_t>(found - kTiers.begin()) + 1;
                }
                // (deeper, lighter): a deeper kind welcome at a lower level than a lighter one. Never is skipped.
                constexpr std::array<std::pair<std::size_t, std::size_t>, 3> pairs = { { { 1, 0 }, { 2, 1 }, { 2, 0 } } };
                for (const auto& [deeper, lighter] : pairs) {
                    const auto& d = kinds[deeper];
                    const auto& l = kinds[lighter];
                    if (d.level != 0 && l.level != 0 && d.level < l.level) {
                        Add(out, Finding::Level::Warning, d.line != 0 ? d.line : LineOf(moments->source()),
                            std::format("moments: {} is welcome before {}", d.key, l.key));
                    }
                }
            }
        }

        // The parser's own text starts with "<fileName>: "; the finding carries the file separately.
        std::string StripSource(std::string message, std::string_view fileName)
        {
            const std::string prefix = std::string(fileName) + ": ";
            if (message.starts_with(prefix)) message.erase(0, prefix.size());
            return message;
        }

        // The line of the key the parser named in "unknown field 'x'", or 0.
        std::int32_t UnknownFieldLine(const toml::table& doc, std::string_view message)
        {
            constexpr std::string_view marker = "unknown field '";
            const auto at = message.find(marker);
            if (at == std::string_view::npos || !message.ends_with('\'')) return 0;
            const auto start = at + marker.size();
            const auto name = message.substr(start, message.size() - start - 1);
            std::int32_t found = 0;
            const auto search = [&](const toml::table& table, auto&& self) -> void {
                for (const auto& [key, value] : table) {
                    if (found == 0 && key.str() == name) found = LineOf(key.source());
                    if (const auto* child = value.as_table()) self(*child, self);
                }
            };
            search(doc, search);
            return found;
        }
    }

    std::vector<Finding> Lint(std::string_view text, std::string_view fileName, bool playerOwned)
    {
        std::vector<Finding> findings;
        std::optional<toml::table> doc;
        std::int32_t syntaxLine = 0;
        try {
            doc = toml::parse(text, fileName);
        } catch (const toml::parse_error& e) {
            syntaxLine = LineOf(e.source());
        } catch (const std::exception&) {
        }

        try {
            (void)ParseProfile(text, fileName, playerOwned);
        } catch (const std::exception& e) {
            auto message = StripSource(e.what(), fileName);
            const auto line = doc ? UnknownFieldLine(*doc, message) : syntaxLine;
            Add(findings, Finding::Level::Error, line, std::move(message));
        }

        if (doc) Warn(findings, *doc, playerOwned);
        std::stable_sort(findings.begin(), findings.end(), [](const Finding& a, const Finding& b) {
            if (a.line != b.line) return a.line < b.line;
            return a.level < b.level;  // Error is declared first
        });
        return findings;
    }
}
