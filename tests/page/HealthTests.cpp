#include "check.h"
#include "health/Health.h"

#include <string>

using namespace romantasy::health;
using json = nlohmann::json;

namespace
{
    const Item* Find(const Report& report, const std::string& id)
    {
        for (const auto& item : report.items) {
            if (item.id == id) return &item;
        }
        return nullptr;
    }
}

TEST(Health_HealthyIsOkAndSilent)
{
    const Facts facts;
    const auto report = BuildReport(facts);
    CHECK(report.status == "ok");
    CHECK(report.items.empty());
    CHECK(!HudLine(report, facts).has_value());
}

TEST(Health_NotesStayQuiet)
{
    Facts facts;
    facts.orphans = 2;
    const auto report = BuildReport(facts);
    CHECK(report.status == "ok");
    CHECK(report.items.size() == 1);
    CHECK(report.items[0].id == "orphans");
    CHECK(report.items[0].level == Level::Note);
    CHECK(report.items[0].title == "Keeping 2 bond(s) for followers whose mods are not loaded");
    CHECK(report.items[0].detail == "They return when the mods do. Nothing is lost.");
    CHECK(!HudLine(report, facts).has_value());
}

TEST(Health_HostMissing)
{
    Facts facts;
    facts.host = HostState::Missing;
    const auto report = BuildReport(facts);
    CHECK(report.status == "error");
    const auto* host = Find(report, "host");
    CHECK(host && host->level == Level::Error);
    CHECK(host && host->title == "Magelight UI is not installed");
    CHECK(host && host->detail == "The ledger needs Magelight UI 1.6 or newer. Points are still tracked.");
    CHECK(HudLine(report, facts) == std::optional<std::string>("Romantasy's ledger needs Magelight UI 1.6 or newer."));
}

TEST(Health_HostTooOld)
{
    Facts facts;
    facts.host = HostState::TooOld;
    facts.hostVersion = "1.5.0";
    const auto report = BuildReport(facts);
    CHECK(report.status == "error");
    const auto* host = Find(report, "host");
    CHECK(host && host->title == "Magelight UI is too old");
    CHECK(host && host->detail.find("Found 1.5.0.") != std::string::npos);
    CHECK(host && host->detail == "Found 1.5.0. The ledger needs 1.6 or newer.");
    CHECK(HudLine(report, facts) == std::optional<std::string>("Romantasy's ledger needs Magelight UI 1.6 or newer."));
}

TEST(Health_HostFailed)
{
    Facts facts;
    facts.host = HostState::Failed;
    const auto report = BuildReport(facts);
    CHECK(report.status == "error");
    const auto* host = Find(report, "host");
    CHECK(host && host->title == "The ledger could not start");
    CHECK(host && host->detail == "Reinstall Romantasy. Details are in Romantasy.log.");
    CHECK(HudLine(report, facts) == std::optional<std::string>("Romantasy's ledger could not start. See Romantasy.log."));
}

TEST(Health_PluginFormsMissing)
{
    Facts facts;
    facts.statFactions = 57;
    auto report = BuildReport(facts);
    CHECK(report.status == "error");
    CHECK(report.items.size() == 1);
    CHECK(report.items[0].id == "plugin");
    CHECK(report.items[0].level == Level::Error);
    CHECK(report.items[0].title == "CS_Romantasy.esp is missing or out of date");
    CHECK(report.items[0].detail == "Enable CS_Romantasy.esp, or reinstall Romantasy if it is already enabled.");

    facts.romanceLevelFaction = false;
    report = BuildReport(facts);
    CHECK(report.items.size() == 1);
    CHECK(report.items[0].id == "plugin");

    Facts points;
    points.romancePointsFaction = false;
    CHECK(BuildReport(points).items.size() == 1);
}

TEST(Health_PluginMissingHidesPower)
{
    // A missing CS_Romantasy.esp is one cause: it takes the power with it.
    Facts facts;
    facts.romanceLevelFaction = false;
    facts.romancePointsFaction = false;
    facts.statFactions = 0;
    facts.power = false;
    const auto report = BuildReport(facts);
    CHECK(report.items.size() == 1);
    CHECK(report.items.size() == 1 && report.items[0].id == "plugin");
    CHECK(HudLine(report, facts) ==
        std::optional<std::string>("Romantasy found 1 problem(s). Open the ledger's Settings to see them."));
}

TEST(Health_ConditionsOff)
{
    for (int which = 0; which < 2; ++which) {
        Facts facts;
        (which == 0 ? facts.graphHook : facts.factionRankHook) = false;
        const auto report = BuildReport(facts);
        CHECK(report.status == "error");
        CHECK(report.items.size() == 1);
        CHECK(report.items[0].id == "conditions");
        CHECK(report.items[0].level == Level::Error);
        CHECK(report.items[0].title == "Dialogue conditions are off");
        CHECK(report.items[0].detail ==
            "This game version is not supported by the dialogue bridge. Romance dialogue may not unlock.");
    }
    Facts both;
    both.graphHook = false;
    both.factionRankHook = false;
    CHECK(BuildReport(both).items.size() == 1);
}

TEST(Health_ProfilesListsThreeThenCounts)
{
    Facts facts;
    facts.profileProblems = { "a.toml: one", "b.toml: two", "c.toml: three", "d.toml: four", "e.toml: five" };
    auto report = BuildReport(facts);
    CHECK(report.status == "warn");
    CHECK(report.items.size() == 1);
    CHECK(report.items[0].id == "profiles");
    CHECK(report.items[0].level == Level::Warn);
    CHECK(report.items[0].title == "5 personality file(s) could not be used");
    CHECK(report.items[0].detail == "a.toml: one\nb.toml: two\nc.toml: three\nand 2 more in Romantasy.log");

    facts.profileProblems = { "a.toml: one", "b.toml: two" };
    report = BuildReport(facts);
    CHECK(report.items[0].title == "2 personality file(s) could not be used");
    CHECK(report.items[0].detail == "a.toml: one\nb.toml: two");
}

TEST(Health_OrdersWorstFirst)
{
    Facts facts;
    facts.saveDamaged = true;   // warn
    facts.orphans = 1;          // note
    facts.statFactions = 0;     // error
    const auto report = BuildReport(facts);
    CHECK(report.status == "error");
    CHECK(report.items.size() == 3);
    CHECK(report.items[0].id == "plugin" && report.items[0].level == Level::Error);
    CHECK(report.items[1].id == "saveDamaged" && report.items[1].level == Level::Warn);
    CHECK(report.items[1].title == "Part of this save's Romantasy data was damaged");
    CHECK(report.items[1].detail == "Everything that could be read was kept. Save again to repair it.");
    CHECK(report.items[2].id == "orphans" && report.items[2].level == Level::Note);

    // Within a level, the table's order.
    Facts many;
    many.host = HostState::Missing;
    many.romanceLevelFaction = false;
    many.graphHook = false;
    many.power = false;
    many.followerFaction = false;
    many.profileProblems = { "a.toml: one" };
    many.saveDamaged = true;
    many.settingsUnsaved = true;
    many.orphans = 3;
    const auto all = BuildReport(many);
    // No "power": the plugin item already covers it.
    const char* expected[] = { "host", "plugin", "conditions", "followerFaction", "profiles", "saveDamaged",
        "settings", "orphans" };
    CHECK(all.items.size() == std::size(expected));
    for (std::size_t i = 0; i < all.items.size() && i < std::size(expected); ++i) {
        CHECK(all.items[i].id == expected[i]);
    }

    many.romanceLevelFaction = true;
    const auto withPower = BuildReport(many);
    const char* expectedWithPower[] = { "host", "conditions", "power", "followerFaction", "profiles", "saveDamaged",
        "settings", "orphans" };
    CHECK(withPower.items.size() == std::size(expectedWithPower));
    for (std::size_t i = 0; i < withPower.items.size() && i < std::size(expectedWithPower); ++i) {
        CHECK(withPower.items[i].id == expectedWithPower[i]);
    }
}

TEST(Health_HudCountsWarnAndError)
{
    Facts facts;
    facts.power = false;
    facts.factionRankHook = false;
    facts.orphans = 4;  // a note: not counted
    const auto report = BuildReport(facts);
    const auto* power = Find(report, "power");
    CHECK(power && power->title == "The Romantasy power is missing");
    CHECK(power && power->detail == "Opening the ledger from the Favorites menu will not work.");
    CHECK(HudLine(report, facts) ==
        std::optional<std::string>("Romantasy found 2 problem(s). Open the ledger's Settings to see them."));

    Facts follower;
    follower.followerFaction = false;
    const auto one = BuildReport(follower);
    CHECK(one.status == "warn");
    CHECK(one.items.size() == 1 && one.items[0].title == "The follower faction was not found");
    CHECK(one.items.size() == 1 &&
        one.items[0].detail == "Romantasy falls back to the teammate flag to tell who is travelling with you.");
    CHECK(HudLine(one, follower) ==
        std::optional<std::string>("Romantasy found 1 problem(s). Open the ledger's Settings to see them."));
}

TEST(Health_SettingsMessages)
{
    Facts unreadable;
    unreadable.settingsUnreadable = true;
    auto report = BuildReport(unreadable);
    CHECK(report.status == "warn");
    CHECK(report.items.size() == 1);
    CHECK(report.items[0].id == "settings" && report.items[0].level == Level::Warn);
    CHECK(report.items[0].title == "Settings could not be read");
    CHECK(report.items[0].detail == "Defaults are in use.");

    Facts unsaved;
    unsaved.settingsUnsaved = true;
    report = BuildReport(unsaved);
    CHECK(report.items.size() == 1);
    CHECK(report.items[0].title == "Settings could not be saved");
    CHECK(report.items[0].detail == "Your choices last until the game closes.");

    Facts both;
    both.settingsUnreadable = true;
    both.settingsUnsaved = true;
    report = BuildReport(both);
    CHECK(report.items.size() == 1);
    CHECK(report.items[0].title == "Settings could not be saved");
    CHECK(report.items[0].detail == "Your choices last until the game closes.");
}

TEST(Health_JsonShape)
{
    Facts facts;
    facts.version = "3.0.1";
    facts.hostVersion = "1.6.2";
    facts.bonds = 2;
    facts.profileProblems = { "a.toml: bad" };
    facts.orphans = 1;
    facts.graphHook = false;
    const auto j = ToJson(BuildReport(facts), facts);
    CHECK(j.is_object());
    CHECK(j.size() == 5);
    CHECK(j["status"] == "error");
    CHECK(j["version"] == "3.0.1");
    CHECK(j["host"] == "1.6.2");
    CHECK(j["bonds"] == 2);
    CHECK(j["items"].is_array() && j["items"].size() == 3);
    CHECK(j["items"][0]["level"] == "error");
    CHECK(j["items"][1]["level"] == "warn");
    CHECK(j["items"][2]["level"] == "note");
    const auto& item = j["items"][1];
    CHECK(item.size() == 4);
    CHECK(item["id"] == "profiles");
    CHECK(item["title"] == "1 personality file(s) could not be used");
    CHECK(item["detail"] == "a.toml: bad");

    const Facts healthy;
    const auto ok = ToJson(BuildReport(healthy), healthy);
    CHECK(ok["status"] == "ok");
    CHECK(ok["host"] == "");
    CHECK(ok["items"].is_array() && ok["items"].empty());
}

TEST(Health_ProfileProblem_PathToFileName)
{
    CHECK(ProfileProblemsFromLoaderError("Data/SKSE/Plugins/Romantasy/Profiles/File.toml: reason") ==
        std::vector<std::string>{ "File.toml: reason" });
    // The reason may itself hold ": ".
    CHECK(ProfileProblemsFromLoaderError("Profiles/a.TOML: line 3: expected '='") ==
        std::vector<std::string>{ "a.TOML: line 3: expected '='" });
}

TEST(Health_ProfileProblem_WindowsPathWithSpaces)
{
    CHECK(ProfileProblemsFromLoaderError(R"(D:\Nordic Souls\Data\Player Profiles\My Follower.toml: cannot open profile)") ==
        std::vector<std::string>{ "My Follower.toml: cannot open profile" });
}

TEST(Health_ProfileProblem_ConflictIsOnePerFile)
{
    const auto problems = ProfileProblemsFromLoaderError(
        R"(conflicting profiles for a.esp|2048; all definitions rejected: Profiles/One.toml Profiles\Two Words.toml)");
    CHECK(problems == (std::vector<std::string>{
        "One.toml: conflicting profiles for a.esp|2048; all definitions rejected",
        "Two Words.toml: conflicting profiles for a.esp|2048; all definitions rejected",
    }));
}

TEST(Health_ProfileProblem_NoTomlPassesThrough)
{
    CHECK(ProfileProblemsFromLoaderError("Data/SKSE/Plugins/Romantasy/Profiles: access denied") ==
        std::vector<std::string>{ "Data/SKSE/Plugins/Romantasy/Profiles: access denied" });
    CHECK(ProfileProblemsFromLoaderError("something went wrong") == std::vector<std::string>{ "something went wrong" });
}

TEST(Health_ProfileProblem_EmptyGivesNothing)
{
    CHECK(ProfileProblemsFromLoaderError("").empty());
}

TEST(Health_AuthorModeNote)
{
    Facts off;
    CHECK(Find(BuildReport(off), "authorMode") == nullptr);

    Facts on;
    on.authorMode = true;
    const auto report = BuildReport(on);
    CHECK(report.status == "ok");
    CHECK(report.items.size() == 1);
    const auto* note = Find(report, "authorMode");
    CHECK(note && note->level == Level::Note);
    CHECK(note && note->title == "Author mode is on.");
    CHECK(!HudLine(report, on).has_value());

    // Notes come after problems.
    on.saveDamaged = true;
    const auto both = BuildReport(on);
    CHECK(both.items.size() == 2 && both.items[0].id == "saveDamaged" && both.items[1].id == "authorMode");
}

TEST(Health_WhispersNote)
{
    // On, the ledger's host is fine, and the HUD card could not be made: a note.
    Facts facts;
    facts.whispers = true;
    facts.whisperView = false;
    const auto report = BuildReport(facts);
    CHECK(report.status == "ok");
    CHECK(report.items.size() == 1);
    const auto* note = Find(report, "whispers");
    CHECK(note && note->level == Level::Note);
    CHECK(note && note->title == "Whispers are using the game's own notifications.");
    CHECK(!HudLine(report, facts).has_value());

    // Magelight absent: still the note, after the error.
    facts.host = HostState::Missing;
    const auto missing = BuildReport(facts);
    CHECK(missing.items.size() == 2 && missing.items[0].id == "host" && missing.items[1].id == "whispers");

    // Too old or failed: the host error says it; no note.
    facts.host = HostState::TooOld;
    CHECK(Find(BuildReport(facts), "whispers") == nullptr);
    facts.host = HostState::Failed;
    CHECK(Find(BuildReport(facts), "whispers") == nullptr);

    // Whispers off, or the card is there: nothing.
    Facts off;
    off.whispers = false;
    off.whisperView = false;
    CHECK(Find(BuildReport(off), "whispers") == nullptr);
    Facts fine;
    fine.whispers = true;
    fine.whisperView = true;
    CHECK(BuildReport(fine).items.empty());
    // The defaults are healthy.
    CHECK(Find(BuildReport(Facts{}), "whispers") == nullptr);
}

TEST(Health_SkyrimNetNotes)
{
    // Not found (the default): nothing, whatever the switch says.
    Facts absent;
    CHECK(absent.skyrimNet == SkyrimNetState::Absent);
    CHECK(Find(BuildReport(absent), "skyrimNet") == nullptr);
    absent.skyrimNetBridge = false;
    CHECK(BuildReport(absent).items.empty());

    // Found: a note saying whether the bridge is on; the status stays ok and there is no HUD line.
    Facts on;
    on.skyrimNet = SkyrimNetState::Found;
    on.skyrimNetBridge = true;
    const auto onReport = BuildReport(on);
    CHECK(onReport.status == "ok" && onReport.items.size() == 1);
    const auto* note = Find(onReport, "skyrimNet");
    CHECK(note && note->level == Level::Note && note->title == "SkyrimNet found: the bridge is on." && note->detail.empty());
    CHECK(!HudLine(onReport, on).has_value());
    Facts off = on;
    off.skyrimNetBridge = false;
    const auto offReport = BuildReport(off);
    const auto* offNote = Find(offReport, "skyrimNet");
    CHECK(offNote && offNote->level == Level::Note && offNote->title == "SkyrimNet found: the bridge is off.");

    // Found but too old: a note with the same words, whatever the switch says; the status stays ok and there is no HUD line.
    for (const bool bridge : { true, false }) {
        Facts old;
        old.skyrimNet = SkyrimNetState::TooOld;
        old.skyrimNetBridge = bridge;
        const auto report = BuildReport(old);
        CHECK(report.status == "ok" && report.items.size() == 1);
        const auto* oldNote = Find(report, "skyrimNet");
        CHECK(oldNote && oldNote->level == Level::Note && oldNote->title == "SkyrimNet is too old for Romantasy's bridge.");
        CHECK(oldNote && oldNote->detail == "Update SkyrimNet to use the bridge. Everything else works without it.");
        CHECK(!HudLine(report, old).has_value());
    }

    // Among other notes it keeps the spec table's order: after whispers, before author mode.
    Facts many = on;
    many.whispers = true;
    many.whisperView = false;
    many.authorMode = true;
    const auto ordered = BuildReport(many);
    CHECK(ordered.items.size() == 3 && ordered.items[0].id == "whispers" && ordered.items[1].id == "skyrimNet" && ordered.items[2].id == "authorMode");
}

TEST(Health_SkyrimNetBondLineMissing)
{
    // Found, bridge on, the decorator refused on the last load: the note says so instead of plain "on".
    Facts facts;
    facts.skyrimNet = SkyrimNetState::Found;
    facts.skyrimNetBridge = true;
    facts.skyrimNetBondLineMissing = true;
    const auto report = BuildReport(facts);
    CHECK(report.status == "ok" && report.items.size() == 1);
    const auto* note = Find(report, "skyrimNet");
    CHECK(note && note->level == Level::Note && note->title == "SkyrimNet found: the bridge is on, but the bond line is not registered.");
    CHECK(!HudLine(report, facts).has_value());
    // Bridge off: the plain off note; too old or absent: unchanged.
    facts.skyrimNetBridge = false;
    const auto off = BuildReport(facts);
    CHECK(off.items.size() == 1 && off.items[0].title == "SkyrimNet found: the bridge is off.");
    facts.skyrimNet = SkyrimNetState::TooOld;
    facts.skyrimNetBridge = true;
    const auto old = BuildReport(facts);
    CHECK(old.items.size() == 1 && old.items[0].title == "SkyrimNet is too old for Romantasy's bridge.");
    facts.skyrimNet = SkyrimNetState::Absent;
    CHECK(BuildReport(facts).items.empty());
    // Not missing: the plain on note.
    Facts fine;
    fine.skyrimNet = SkyrimNetState::Found;
    CHECK(!fine.skyrimNetBondLineMissing);
    const auto fineReport = BuildReport(fine);
    CHECK(fineReport.items.size() == 1 && fineReport.items[0].title == "SkyrimNet found: the bridge is on.");
}

TEST(Health_OStimNotes)
{
    // Absent (the default): nothing, whatever the switch says.
    Facts absent;
    CHECK(absent.ostim == OStimState::Absent && absent.ostimMoments);
    CHECK(BuildReport(absent).items.empty());
    absent.ostimMoments = false;
    CHECK(BuildReport(absent).items.empty());

    // Found, moments on: a note; the status stays ok and there is no HUD line.
    Facts on;
    on.ostim = OStimState::Found;
    on.ostimMoments = true;
    const auto onReport = BuildReport(on);
    CHECK(onReport.status == "ok" && onReport.items.size() == 1);
    const auto* note = Find(onReport, "ostim");
    CHECK(note && note->level == Level::Note && note->title == "OStim found: shared moments count toward a bond.");
    CHECK(note && note->detail == "Once a day, time alone with a companion adds a little to the bond.");
    CHECK(!HudLine(onReport, on).has_value());

    // Found, moments off.
    Facts off = on;
    off.ostimMoments = false;
    const auto offReport = BuildReport(off);
    CHECK(offReport.status == "ok" && offReport.items.size() == 1);
    const auto* offNote = Find(offReport, "ostim");
    CHECK(offNote && offNote->level == Level::Note && offNote->title == "OStim found: shared moments are not counted.");
    CHECK(offNote && offNote->detail == "Turn on Shared moments in Settings to count them.");
    CHECK(!HudLine(offReport, off).has_value());

    // Too old: the same words whatever the switch says.
    for (const bool moments : { true, false }) {
        Facts old;
        old.ostim = OStimState::TooOld;
        old.ostimMoments = moments;
        const auto report = BuildReport(old);
        CHECK(report.status == "ok" && report.items.size() == 1);
        const auto* oldNote = Find(report, "ostim");
        CHECK(oldNote && oldNote->level == Level::Note && oldNote->title == "OStim is too old for Romantasy's bridge.");
        CHECK(oldNote && oldNote->detail == "It needs OStim 7.3.3 or newer. Everything else works without it.");
        CHECK(!HudLine(report, old).has_value());
    }

    // Among other notes: after skyrimNet, before authorMode.
    Facts many = on;
    many.whispers = true;
    many.whisperView = false;
    many.skyrimNet = SkyrimNetState::Found;
    many.authorMode = true;
    const auto ordered = BuildReport(many);
    CHECK(ordered.status == "ok" && ordered.items.size() == 4 && ordered.items[0].id == "whispers" && ordered.items[1].id == "skyrimNet" &&
        ordered.items[2].id == "ostim" && ordered.items[3].id == "authorMode");
    CHECK(!HudLine(ordered, many).has_value());
    // Beside a warning it never changes the status or the HUD line.
    Facts warned = on;
    warned.settingsUnreadable = true;
    Facts warnedOnly;
    warnedOnly.settingsUnreadable = true;
    const auto warnedReport = BuildReport(warned);
    CHECK(warnedReport.status == "warn" && warnedReport.items.back().id == "ostim");
    CHECK(HudLine(warnedReport, warned) == HudLine(BuildReport(warnedOnly), warnedOnly));
}
