#include "health/Health.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <format>

namespace romantasy::health
{
    namespace
    {
        constexpr int kStatFactions = 58;
        constexpr std::size_t kListedProfileProblems = 3;

        std::string ProfilesDetail(const std::vector<std::string>& problems)
        {
            std::string detail;
            const auto listed = (std::min)(problems.size(), kListedProfileProblems);
            for (std::size_t i = 0; i < listed; ++i) {
                if (i > 0) detail += '\n';
                detail += problems[i];
            }
            if (problems.size() > listed) {
                detail += std::format("\nand {} more in Romantasy.log", problems.size() - listed);
            }
            return detail;
        }

        bool EndsWithToml(std::string_view text)
        {
            constexpr std::string_view kToml = ".toml";
            if (text.size() < kToml.size()) return false;
            const auto tail = text.substr(text.size() - kToml.size());
            return std::ranges::equal(tail, kToml, [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == b;
            });
        }

        std::string_view FileName(std::string_view path)
        {
            const auto slash = path.find_last_of("/\\");
            return slash == std::string_view::npos ? path : path.substr(slash + 1);
        }
    }

    std::vector<std::string> ProfileProblemsFromLoaderError(std::string_view error)
    {
        if (error.empty()) return {};
        constexpr std::string_view kConflict = "conflicting profiles for ";
        constexpr std::string_view kRejected = "; all definitions rejected:";
        if (error.starts_with(kConflict)) {
            if (const auto rejected = error.find(kRejected); rejected != std::string_view::npos) {
                const auto reason = error.substr(0, rejected + kRejected.size() - 1);  // without the final ':'
                std::vector<std::string> problems;
                // The sources are joined by spaces and each ends in ".toml" (any case); a name may hold spaces.
                auto rest = error.substr(rejected + kRejected.size());
                while (!rest.empty()) {
                    auto end = rest.size();
                    for (std::size_t at = 0; at < rest.size(); ++at) {
                        if (rest[at] == ' ' && EndsWithToml(rest.substr(0, at))) {
                            end = at;
                            break;
                        }
                    }
                    auto source = rest.substr(0, end);
                    while (source.starts_with(' ')) source.remove_prefix(1);
                    if (!source.empty()) {
                        problems.push_back(std::string(FileName(source)) + ": " + std::string(reason));
                    }
                    rest.remove_prefix(end);
                }
                if (!problems.empty()) return problems;
            }
            return { std::string(error) };
        }
        const auto colon = error.find(": ");
        if (colon == std::string_view::npos || !EndsWithToml(error.substr(0, colon))) return { std::string(error) };
        return { std::string(FileName(error.substr(0, colon))) + std::string(error.substr(colon)) };
    }

    const char* LevelName(Level level)
    {
        switch (level) {
        case Level::Error: return "error";
        case Level::Warn:  return "warn";
        default:           return "note";
        }
    }

    Report BuildReport(const Facts& facts)
    {
        Report report;
        auto& items = report.items;

        // The spec table's order; sorted by level below.
        switch (facts.host) {
        case HostState::Missing:
            items.push_back({ "host", Level::Error, "Magelight UI is not installed",
                "The ledger needs Magelight UI 1.6 or newer. Points are still tracked." });
            break;
        case HostState::TooOld:
            items.push_back({ "host", Level::Error, "Magelight UI is too old",
                std::format("Found {}. The ledger needs 1.6 or newer.", facts.hostVersion) });
            break;
        case HostState::Failed:
            items.push_back({ "host", Level::Error, "The ledger could not start",
                "Reinstall Romantasy. Details are in Romantasy.log." });
            break;
        default:
            break;
        }
        const bool pluginBroken = !facts.romanceLevelFaction || !facts.romancePointsFaction || facts.statFactions < kStatFactions;
        if (pluginBroken) {
            items.push_back({ "plugin", Level::Error, "CS_Romantasy.esp is missing or out of date",
                "Enable CS_Romantasy.esp, or reinstall Romantasy if it is already enabled." });
        }
        // The power lives in CS_Romantasy.esp: with the plugin reported, its absence is the same cause.
        if (!facts.power && !pluginBroken) {
            items.push_back({ "power", Level::Warn, "The Romantasy power is missing",
                "Opening the ledger from the Favorites menu will not work." });
        }
        if (!facts.graphHook || !facts.factionRankHook) {
            items.push_back({ "conditions", Level::Error, "Dialogue conditions are off",
                "This game version is not supported by the dialogue bridge. Romance dialogue may not unlock." });
        }
        if (!facts.followerFaction) {
            items.push_back({ "followerFaction", Level::Warn, "The follower faction was not found",
                "Romantasy falls back to the teammate flag to tell who is travelling with you." });
        }
        if (!facts.profileProblems.empty()) {
            items.push_back({ "profiles", Level::Warn,
                std::format("{} personality file(s) could not be used", facts.profileProblems.size()),
                ProfilesDetail(facts.profileProblems) });
        }
        if (facts.saveDamaged) {
            items.push_back({ "saveDamaged", Level::Warn, "Part of this save's Romantasy data was damaged",
                "Everything that could be read was kept. Save again to repair it." });
        }
        if (facts.settingsUnsaved) {
            items.push_back({ "settings", Level::Warn, "Settings could not be saved",
                "Your choices last until the game closes." });
        } else if (facts.settingsUnreadable) {
            items.push_back({ "settings", Level::Warn, "Settings could not be read", "Defaults are in use." });
        }
        if (facts.orphans > 0) {
            items.push_back({ "orphans", Level::Note,
                std::format("Keeping {} bond(s) for followers whose mods are not loaded", facts.orphans),
                "They return when the mods do. Nothing is lost." });
        }
        // The ledger's host is fine (or absent, said above) but the HUD card is not there: the fallback is in use.
        // Too old or failed, the host error already says why.
        if (facts.whispers && !facts.whisperView && (facts.host == HostState::Ok || facts.host == HostState::Missing)) {
            items.push_back({ "whispers", Level::Note, "Whispers are using the game's own notifications.", "" });
        }
        // SkyrimNet is optional: absent says nothing; found says whether the bridge is on; too old is a note too, so an
        // optional mod never raises the HUD line.
        if (facts.skyrimNet == SkyrimNetState::Found) {
            items.push_back({ "skyrimNet", Level::Note,
                !facts.skyrimNetBridge ? "SkyrimNet found: the bridge is off."
                    : facts.skyrimNetBondLineMissing ? "SkyrimNet found: the bridge is on, but the bond line is not registered."
                    : "SkyrimNet found: the bridge is on.", "" });
        } else if (facts.skyrimNet == SkyrimNetState::TooOld) {
            items.push_back({ "skyrimNet", Level::Note, "SkyrimNet is too old for Romantasy's bridge.",
                "Update SkyrimNet to use the bridge. Everything else works without it." });
        }
        // OStim is optional too: absent says nothing; found says whether shared moments count; too old is a note.
        if (facts.ostim == OStimState::Found) {
            items.push_back(facts.ostimMoments
                ? Item{ "ostim", Level::Note, "OStim found: shared moments count toward a bond.",
                      "Once a day, time alone with a companion adds a little to the bond." }
                : Item{ "ostim", Level::Note, "OStim found: shared moments are not counted.",
                      "Turn on Shared moments in Settings to count them." });
        } else if (facts.ostim == OStimState::TooOld) {
            items.push_back({ "ostim", Level::Note, "OStim is too old for Romantasy's bridge.",
                "It needs OStim 7.3.3 or newer. Everything else works without it." });
        }
        if (facts.authorMode) {
            items.push_back({ "authorMode", Level::Note, "Author mode is on.",
                "The debug functions for follower authors work. Set authorMode to false in settings.json when you are done." });
        }

        std::ranges::stable_sort(items, [](const Item& lhs, const Item& rhs) { return lhs.level > rhs.level; });

        report.status = "ok";
        if (!items.empty() && items.front().level != Level::Note) {
            report.status = LevelName(items.front().level);
        }
        return report;
    }

    nlohmann::json ToJson(const Report& report, const Facts& facts)
    {
        nlohmann::json items = nlohmann::json::array();
        for (const auto& item : report.items) {
            items.push_back({
                { "id", item.id },
                { "level", LevelName(item.level) },
                { "title", item.title },
                { "detail", item.detail },
            });
        }
        return {
            { "status", report.status },
            { "version", facts.version },
            { "host", facts.hostVersion },
            { "bonds", facts.bonds },
            { "items", std::move(items) },
        };
    }

    std::optional<std::string> HudLine(const Report& report, const Facts& facts)
    {
        switch (facts.host) {
        case HostState::Missing:
        case HostState::TooOld:
            return "Romantasy's ledger needs Magelight UI 1.6 or newer.";
        case HostState::Failed:
            return "Romantasy's ledger could not start. See Romantasy.log.";
        default:
            break;
        }
        const auto problems = std::ranges::count_if(report.items, [](const Item& item) { return item.level != Level::Note; });
        if (problems == 0) return std::nullopt;
        return std::format("Romantasy found {} problem(s). Open the ledger's Settings to see them.", problems);
    }
}
