#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

// The health report: plain facts about the install in, a list of problems out. Pure; no Skyrim headers.
namespace romantasy::health
{
    enum class Level { Note, Warn, Error };
    enum class HostState { Ok, Missing, TooOld, Failed };
    enum class SkyrimNetState { Absent, TooOld, Found };
    enum class OStimState { Absent, TooOld, Found };

    struct Facts
    {
        std::string version;                 // Romantasy version, e.g. "3.0.1"
        HostState host = HostState::Ok;
        std::string hostVersion;             // e.g. "1.6.2"; empty when unknown
        bool romanceLevelFaction = true, romancePointsFaction = true;
        int statFactions = 58;               // of 58
        bool power = true, graphHook = true, factionRankHook = true, followerFaction = true;
        std::vector<std::string> profileProblems;   // "file: reason"
        bool saveDamaged = false;
        int orphans = 0;                     // the larger of orphaned points entries and orphaned bonds
        bool settingsUnreadable = false, settingsUnsaved = false;
        int bonds = 0;
        bool authorMode = false;             // settings.json "authorMode": the debug functions work
        bool whispers = true;                // settings.json "whispers"
        bool whisperView = true;             // the whisper HUD view was made and is alive
        SkyrimNetState skyrimNet = SkyrimNetState::Absent;  // whether SkyrimNet.dll is loaded, and new enough for the bridge
        bool skyrimNetBridge = true;         // settings.json "skyrimNetBridge"
        bool skyrimNetBondLineMissing = false;  // SkyrimNet refused the romantasy_bond decorator on the last load
        OStimState ostim = OStimState::Absent;  // whether OStim handed out its thread interface, or is loaded but too old
        bool ostimMoments = true;            // settings.json "ostimMoments"
    };

    struct Item
    {
        std::string id;
        Level level;
        std::string title;
        std::string detail;
    };

    struct Report
    {
        std::string status;  // "ok" | "warn" | "error"
        std::vector<Item> items;
    };

    // Items worst first, and within a level in the spec table's order.
    [[nodiscard]] Report BuildReport(const Facts& facts);
    // { status, version, host, bonds, items: [ { id, level, title, detail } ] }; levels "note", "warn", "error".
    [[nodiscard]] nlohmann::json ToJson(const Report& report, const Facts& facts);
    // The one-time HUD line, or nullopt when there is nothing worth a HUD line (notes never are).
    [[nodiscard]] std::optional<std::string> HudLine(const Report& report, const Facts& facts);
    [[nodiscard]] const char* LevelName(Level level);
    // Facts::profileProblems entries ("<file name>: <reason>") from one profile loader error. An error that starts with
    // a .toml file's path keeps only the file name; a conflict naming several files gives one entry per file; any
    // other error is passed through unchanged; an empty one gives nothing.
    [[nodiscard]] std::vector<std::string> ProfileProblemsFromLoaderError(std::string_view error);
}
