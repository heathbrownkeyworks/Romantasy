#include "check.h"
#include "romance/RomanceProfiles.h"
#include "romance/SavedRomancePoints.h"
#include <Windows.h>

#include <chrono>
#include <fstream>
#include <stdexcept>

using namespace romantasy;

namespace
{
    const std::string valid = R"(schema_version = 1
[npc]
plugin = "ExampleFollower.esp"
form_id = "0x000800"
[romance]
starting_level = "Friend"
likes = ["Quests Completed", "ROM_DungeonsCleared"]
dislikes = ["Murders"]
)";

    bool Rejects(const std::string& text)
    {
        try { (void)ParseProfile(text, "test.toml"); }
        catch (const std::exception&) { return true; }
        return false;
    }

    std::string Replace(std::string text, std::string_view from, std::string_view to)
    {
        text.replace(text.find(from), from.size(), to);
        return text;
    }

    struct TempDirectory
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("romantasy-profile-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TempDirectory() { std::filesystem::create_directories(path); }
        ~TempDirectory() { std::filesystem::remove_all(path); }
        void Write(const char* name, std::string_view text) { std::ofstream(path / name) << text; }
    };
}

TEST(profile_resolves_labels_and_stable_ids)
{
    const auto p = ParseProfile(valid, "test.toml");
    CHECK(p.plugin == "ExampleFollower.esp");
    CHECK(p.localID == 0x800);
    CHECK(p.startingLevel == 3);
    CHECK(p.preferences.size() == 3);
    CHECK(p.preferences.at("ROM_QuestsCompleted") == 1);
    CHECK(p.preferences.at("ROM_Murders") == -1);
}

TEST(profile_rejects_invalid_identity_and_schema)
{
    CHECK(Rejects(Replace(valid, "schema_version = 1", "schema_version = 2")));
    CHECK(Rejects(Replace(valid, "schema_version = 1", "schema_version = 1.0")));
    CHECK(Rejects(Replace(valid, "0x000800", "0xFE000800")));
    CHECK(Rejects(Replace(valid, "0x000800", "0x000000")));
    CHECK(Rejects(Replace(valid, "0x000800", "0x800garbage")));
    CHECK(Rejects(Replace(valid, "ExampleFollower.esp", "../ExampleFollower.esp")));
    CHECK(Rejects(Replace(valid, "ExampleFollower.esp", "ExampleFollower.txt")));
    CHECK(Rejects(Replace(valid, "Friend", "Best Friend")));
    CHECK(Rejects(Replace(valid, "likes =", "lieks =")));
}

TEST(profile_rejects_unknown_duplicate_and_conflicting_opinions)
{
    CHECK(Rejects(Replace(valid, "Quests Completed", "Unknown Stat")));
    CHECK(Rejects(Replace(valid, "ROM_DungeonsCleared", "ROM_QuestsCompleted")));
    CHECK(Rejects(Replace(valid, "Murders", "Quests Completed")));
    CHECK(Rejects(Replace(valid, "[\"Murders\"]", "[12]")));
    CHECK(Rejects(Replace(valid, "[\"Murders\"]", "\"Murders\"")));
}

TEST(profile_loader_keeps_valid_files_after_cold_start_errors)
{
    TempDirectory temp;
    temp.Write("a-broken.toml", "[npc");
    temp.Write("b-valid.toml", valid);
    temp.Write("c-inert.toml.example", "invalid");
    const auto loaded = LoadProfiles(temp.path);
    CHECK(loaded.profiles.size() == 1);
    CHECK(loaded.errors.size() == 1);
    CHECK(loaded.profiles.front().localID == 0x800);
}

TEST(profile_loader_rejects_all_duplicate_definitions)
{
    TempDirectory temp;
    temp.Write("a.toml", valid);
    temp.Write("b.toml", Replace(valid, "ExampleFollower.esp", "examplefollower.ESP"));
    temp.Write("c.toml", Replace(valid, "0x000800", "0x000801"));
    const auto loaded = LoadProfiles(temp.path);
    CHECK(loaded.profiles.size() == 1);
    CHECK(loaded.profiles.front().localID == 0x801);
    CHECK(loaded.errors.size() == 1);
}

TEST(profile_loader_missing_directory_is_an_empty_install)
{
    TempDirectory temp;
    const auto loaded = LoadProfiles(temp.path / "missing");
    CHECK(loaded.profiles.empty());
    CHECK(loaded.errors.empty());
}

TEST(player_profile_roundtrip_edit_remove_and_reenable)
{
    TempDirectory temp;
    auto profile = ParseProfile(valid, "example.toml");
    profile.playerOwned = true;
    profile.source.clear();
    profile.startingLevel = 1;
    const auto saved = SavePlayerProfile(temp.path, profile);
    CHECK(saved.playerOwned);
    CHECK(saved.source.find("examplefollower.esp.000800.toml") != std::string::npos);
    auto loaded = LoadProfiles(temp.path, true);
    CHECK(loaded.errors.empty());
    CHECK(loaded.profiles.size() == 1);
    CHECK(loaded.profiles.front().preferences == profile.preferences);
    CHECK(loaded.profiles.front().playerOwned);

    profile.preferences = {{"ROM_Murders", 1}};
    (void)SavePlayerProfile(temp.path, profile);
    loaded = LoadProfiles(temp.path, true);
    CHECK(loaded.profiles.front().preferences == profile.preferences);
    profile.enabled = false;
    (void)SavePlayerProfile(temp.path, profile);
    loaded = LoadProfiles(temp.path, true);
    CHECK(loaded.profiles.size() == 1);
    CHECK(!loaded.profiles.front().enabled);
    profile.enabled = true;
    profile.preferences.clear();
    (void)SavePlayerProfile(temp.path, profile);
    loaded = LoadProfiles(temp.path, true);
    CHECK(loaded.profiles.front().enabled);
    CHECK(loaded.profiles.front().preferences.empty());
}

TEST(player_write_failures_preserve_existing_file)
{
    TempDirectory temp;
    auto profile = ParseProfile(valid, "example.toml");
    profile.playerOwned = true;
    profile.source.clear();
    const auto saved = SavePlayerProfile(temp.path, profile);
    const auto fails = [&](const NpcProfile& candidate) {
        try { (void)SavePlayerProfile(temp.path, candidate); }
        catch (const std::exception&) { return true; }
        return false;
    };
    auto invalid = profile;
    invalid.preferences["ROM_Murders"] = 7;
    CHECK(fails(invalid));
    invalid = profile;
    invalid.plugin = "../escape.esp";
    CHECK(fails(invalid));
    invalid = profile;
    invalid.playerOwned = false;
    CHECK(fails(invalid));
    // A directory at the staging path deterministically forces a write failure.
    std::filesystem::create_directory(saved.source + ".tmp");
    invalid = profile;
    invalid.preferences.clear();
    CHECK(fails(invalid));
    const auto loaded = LoadProfiles(temp.path, true);
    CHECK(loaded.profiles.size() == 1);
    CHECK(loaded.profiles.front().preferences == profile.preferences);
}

TEST(player_ownership_and_disabled_state_cannot_be_claimed_by_author_file)
{
    CHECK(Rejects(Replace(valid, "schema_version = 1", "schema_version = 1\nplayerOwned = true")));
    CHECK(Rejects(Replace(valid, "[romance]", "[romance]\nenabled = false")));
    const auto disabled = ParseProfile(Replace(valid, "[romance]", "[romance]\nenabled = false"), "player.toml", true);
    CHECK(disabled.playerOwned && !disabled.enabled);
    bool rejected = false;
    try { (void)ParseProfile(Replace(valid, "[romance]", "[romance]\nenabled = 0"), "player.toml", true); }
    catch (const std::exception&) { rejected = true; }
    CHECK(rejected);
}

TEST(player_writer_preserves_custom_filename_and_rejects_wrong_destination)
{
    TempDirectory temp;
    temp.Write("custom.toml", valid);
    auto profile = LoadProfiles(temp.path, true).profiles.front();
    profile.preferences.clear();
    const auto saved = SavePlayerProfile(temp.path, profile);
    CHECK(std::filesystem::path(saved.source).filename() == "custom.toml");
    CHECK(LoadProfiles(temp.path, true).profiles.size() == 1);
    const auto fails = [&](const NpcProfile& candidate) {
        try { (void)SavePlayerProfile(temp.path, candidate); }
        catch (const std::exception&) { return true; }
        return false;
    };
    profile.localID = 0x801;
    CHECK(fails(profile));
    profile.localID = 0x800;
    profile.source = (temp.path.parent_path() / "escape.toml").string();
    CHECK(fails(profile));
    CHECK(LoadProfiles(temp.path, true).profiles.front().localID == 0x800);
}

TEST(player_files_distinguish_plugins_and_local_ids)
{
    TempDirectory temp;
    auto profile = ParseProfile(valid, "", true);
    (void)SavePlayerProfile(temp.path, profile);
    profile.plugin = "Another NPC.esl";
    (void)SavePlayerProfile(temp.path, profile);
    profile.localID = 0x801;
    (void)SavePlayerProfile(temp.path, profile);
    const auto loaded = LoadProfiles(temp.path, true);
    CHECK(loaded.errors.empty());
    CHECK(loaded.profiles.size() == 3);
}

TEST(player_edit_persists_without_resetting_save_specific_points)
{
    TempDirectory temp;
    auto profile = ParseProfile(valid, "", true);
    profile.startingLevel = 1;
    (void)SavePlayerProfile(temp.path, profile);
    const std::unordered_map<std::uint32_t, std::int32_t> firstSave = {{0x1234, 1014}};
    const std::unordered_map<std::uint32_t, std::int32_t> secondSave = {{0x1234, 0}};
    profile.preferences = {{"ROM_DungeonsCleared", -1}};
    const auto committed = SavePlayerProfile(temp.path, profile);
    CHECK(committed.preferences.at("ROM_DungeonsCleared") == -1);
    // Simulate a cold loader rather than relying on the writer's in-memory result.
    const auto reloaded = LoadProfiles(temp.path, true).profiles.front();
    CHECK(reloaded.preferences == committed.preferences);
    const auto initial = (reloaded.startingLevel - 1) * 500;
    CHECK(RestorePoints(firstSave, 0x1234, 0x800, true, initial) == 1014);
    CHECK(RestorePoints(secondSave, 0x1234, 0x800, true, initial) == 0);
    CHECK(RestorePoints({}, 0x1234, 0x800, true, initial) == 0);
}

TEST(player_atomic_replace_failure_keeps_previous_preferences)
{
    TempDirectory temp;
    auto profile = ParseProfile(valid, "", true);
    const auto saved = SavePlayerProfile(temp.path, profile);
    const auto path = std::filesystem::path(saved.source);
    // Permit reading, but deny rename/delete to exercise the commit failure path.
    const auto handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    CHECK(handle != INVALID_HANDLE_VALUE);
    if (handle == INVALID_HANDLE_VALUE) return;
    profile.preferences.clear();
    bool rejected = false;
    try { (void)SavePlayerProfile(temp.path, profile); }
    catch (const std::exception&) { rejected = true; }
    CloseHandle(handle);
    CHECK(rejected);
    CHECK(LoadProfiles(temp.path, true).profiles.front().preferences == saved.preferences);
}
