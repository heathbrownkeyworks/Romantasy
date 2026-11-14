#include "check.h"
#include "romance/RomanceProfiles.h"
#include "romance/SavedRomancePoints.h"
#include <Windows.h>

#include <chrono>
#include <fstream>
#include <limits>
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

    // True when parsing throws and the message contains `fragment`.
    bool RejectsWith(const std::string& text, std::string_view fragment, bool playerOwned = false)
    {
        try { (void)ParseProfile(text, "test.toml", playerOwned); }
        catch (const std::exception& e) { return std::string_view(e.what()).find(fragment) != std::string_view::npos; }
        return false;
    }

    std::string V2(const std::string& romance, const std::string& extra = "")
    {
        return "schema_version = 2\n[npc]\nplugin = \"ExampleFollower.esp\"\nform_id = \"0x000800\"\n[romance]\n" + romance + "\n" + extra;
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
    CHECK(Rejects(Replace(valid, "schema_version = 1", "schema_version = 4")));
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

TEST(profile_v2_reads_favorites_and_pet_peeves)
{
    const auto p = ParseProfile(V2("likes = [\"Quests Completed\"]\nfavorites = [\"Dungeons Cleared\"]\n"
        "dislikes = [\"Items Stolen\"]\npet_peeves = [\"Murders\"]"), "test.toml");
    CHECK(p.preferences.size() == 4);
    CHECK(p.preferences.at("ROM_QuestsCompleted") == 1);
    CHECK(p.preferences.at("ROM_DungeonsCleared") == 2);
    CHECK(p.preferences.at("ROM_ItemsStolen") == -1);
    CHECK(p.preferences.at("ROM_Murders") == -2);
}

TEST(profile_v1_rejects_v2_fields)
{
    CHECK(RejectsWith(Replace(valid, "dislikes =", "favorites = [\"Barters\"]\ndislikes ="), "unknown field 'favorites'"));
    CHECK(RejectsWith(Replace(valid, "dislikes =", "pet_peeves = [\"Barters\"]\ndislikes ="), "unknown field 'pet_peeves'"));
    CHECK(RejectsWith(valid + "[unlocks]\nFriend = [\"x\"]\n", "unknown field 'unlocks'"));
    const auto p = ParseProfile(valid, "test.toml");
    CHECK(p.preferences.size() == 3);
    CHECK(p.preferences.at("ROM_DungeonsCleared") == 1);
    CHECK(p.preferences.at("ROM_Murders") == -1);
}

TEST(profile_rejects_schema_other_than_1_2_or_3)
{
    CHECK(RejectsWith(Replace(valid, "schema_version = 1", "schema_version = 0"), "schema_version must be integer 1, 2 or 3"));
    CHECK(RejectsWith(Replace(valid, "schema_version = 1", "schema_version = 4"), "schema_version must be integer 1, 2 or 3"));
    CHECK(RejectsWith(Replace(valid, "schema_version = 1", "schema_version = \"2\""), "schema_version must be integer 1, 2 or 3"));
    CHECK(!Rejects(Replace(valid, "schema_version = 1", "schema_version = 3")));
}

TEST(profile_v2_rejects_conflicts_and_caps)
{
    CHECK(RejectsWith(V2("likes = [\"Barters\"]\nfavorites = [\"Barters\"]"), "duplicate or conflicting preference"));
    CHECK(RejectsWith(V2("favorites = [\"Barters\", \"Bribes\", \"Persuasions\", \"Intimidations\"]"), "favorites lists more than 3 statistics"));
    CHECK(RejectsWith(V2("pet_peeves = [\"Barters\", \"Bribes\", \"Persuasions\", \"Intimidations\"]"), "pet_peeves lists more than 3 statistics"));
    const auto p = ParseProfile(V2("favorites = [\"Barters\", \"Bribes\", \"Persuasions\"]\n"
        "pet_peeves = [\"Intimidations\", \"Murders\", \"Items Stolen\"]"), "test.toml");
    CHECK(p.preferences.size() == 6);
}

TEST(profile_v2_reads_unlocks)
{
    const auto p = ParseProfile(V2("likes = [\"Barters\"]",
        "[unlocks]\nFriend = [\"She tells you about Bruma\"]\nLover = [\"A key to her house\", \"Her mother's ring\"]\n"), "test.toml");
    CHECK(p.unlocks[2].size() == 1);
    CHECK(p.unlocks[2][0] == "She tells you about Bruma");
    CHECK(p.unlocks[4].size() == 2);
    CHECK(p.unlocks[4][1] == "Her mother's ring");
    CHECK(p.unlocks[0].empty() && p.unlocks[1].empty() && p.unlocks[3].empty() && p.unlocks[5].empty());
}

TEST(profile_v2_rejects_bad_unlocks)
{
    const auto with = [](const std::string& unlocks) { return V2("likes = [\"Barters\"]", "[unlocks]\n" + unlocks + "\n"); };
    const std::string listMsg = "unlocks for Friend must be a list of 1 to 4 lines";
    const std::string lineMsg = "an unlock line for Friend must be 1 to 60 characters with no control characters";
    CHECK(RejectsWith(with("Stranger = [\"x\"]"), "[unlocks] cannot name Stranger"));
    CHECK(RejectsWith(with("Bestie = [\"x\"]"), "unknown tier 'Bestie' in [unlocks]"));
    CHECK(RejectsWith(with("Friend = []"), listMsg));
    CHECK(RejectsWith(with("Friend = [\"a\", \"b\", \"c\", \"d\", \"e\"]"), listMsg));
    CHECK(RejectsWith(with("Friend = [3]"), listMsg));
    CHECK(RejectsWith(with("Friend = [\"\"]"), lineMsg));
    CHECK(RejectsWith(with("Friend = [\"" + std::string(61, 'a') + "\"]"), lineMsg));
    CHECK(RejectsWith(with("Friend = [\"a\\tb\"]"), lineMsg));
    CHECK(RejectsWith(with("Friend = [\"a\\nb\"]"), lineMsg));
    std::string accented;
    for (int i = 0; i < 60; ++i) accented += "\xC3\xA9";
    const auto p = ParseProfile(with("Friend = [\"" + accented + "\"]"), "test.toml");
    CHECK(p.unlocks[2].size() == 1);
    CHECK(p.unlocks[2].size() == 1 && p.unlocks[2][0] == accented);
    CHECK(RejectsWith(with("Friend = [\"" + accented + "e\"]"), lineMsg));
}

TEST(player_profile_rejects_unlocks)
{
    CHECK(RejectsWith(V2("likes = [\"Barters\"]", "[unlocks]\nFriend = [\"x\"]\n"),
        "[unlocks] is not allowed in a player profile", true));
}

namespace
{
    std::string V3(const std::string& moments, const std::string& extra = "")
    {
        return "schema_version = 3\n[npc]\nplugin = \"ExampleFollower.esp\"\nform_id = \"0x000800\"\n[romance]\nlikes = [\"Barters\"]\n" +
               extra + "[moments]\n" + moments + "\n";
    }
}

TEST(profile_moments_default_when_absent)
{
    const romantasy::ostim::Welcome defaults;
    CHECK(ParseProfile(valid, "test.toml").moments == defaults);
    CHECK(ParseProfile(V2("likes = [\"Barters\"]"), "test.toml").moments == defaults);
    CHECK(ParseProfile(Replace(valid, "schema_version = 1", "schema_version = 3"), "test.toml").moments == defaults);
    CHECK(ParseProfile(V3(""), "test.toml").moments == defaults);
    CHECK(defaults.affection == 3 && defaults.closeness == 4 && defaults.intimacy == 5);
    CHECK(defaults.feeling == romantasy::ostim::Feeling::Warm);
}

TEST(profile_v3_reads_moments)
{
    const auto p = ParseProfile(V3("affection = \"Acquaintance\"\ncloseness = \"Lover\"\nintimacy = \"Spouse\"\nfeeling = \"treasured\""), "test.toml");
    CHECK(p.moments.affection == 2);
    CHECK(p.moments.closeness == 5);
    CHECK(p.moments.intimacy == 6);
    CHECK(p.moments.feeling == romantasy::ostim::Feeling::Treasured);
    CHECK(ParseProfile(V3("affection = \"Stranger\""), "test.toml").moments.affection == 1);
    CHECK(ParseProfile(V3("feeling = \"reserved\""), "test.toml").moments.feeling == romantasy::ostim::Feeling::Reserved);
    CHECK(ParseProfile(V3("feeling = \"warm\""), "test.toml").moments.feeling == romantasy::ostim::Feeling::Warm);
    // A key left out keeps its default.
    const auto partial = ParseProfile(V3("closeness = \"Friend\""), "test.toml").moments;
    CHECK(partial.affection == 3 && partial.closeness == 3 && partial.intimacy == 5);
}

TEST(profile_moments_never_is_stored_as_zero)
{
    const auto p = ParseProfile(V3("affection = \"never\"\ncloseness = \"never\"\nintimacy = \"never\""), "test.toml");
    CHECK(p.moments.affection == 0 && p.moments.closeness == 0 && p.moments.intimacy == 0);
}

TEST(profile_v3_reads_unlocks_and_moments_together)
{
    const auto p = ParseProfile(V3("intimacy = \"never\"", "[unlocks]\nFriend = [\"She tells you about Bruma\"]\n"), "test.toml");
    CHECK(p.unlocks[2].size() == 1 && p.unlocks[2][0] == "She tells you about Bruma");
    CHECK(p.moments.intimacy == 0);
    CHECK(p.moments.affection == 3);
    // Version 3 keeps the version 2 fields.
    const auto strong = ParseProfile(Replace(V2("favorites = [\"Barters\"]\npet_peeves = [\"Murders\"]"), "schema_version = 2", "schema_version = 3"), "test.toml");
    CHECK(strong.preferences.at("ROM_Barters") == 2 && strong.preferences.at("ROM_Murders") == -2);
}

TEST(profile_v3_rejects_bad_moments)
{
    CHECK(RejectsWith(V3("affection = \"Freind\""), "unknown tier 'Freind' in [moments]"));
    CHECK(RejectsWith(V3("closeness = \"friend\""), "unknown tier 'friend' in [moments]"));
    CHECK(RejectsWith(V3("intimacy = \"Never\""), "unknown tier 'Never' in [moments]"));
    CHECK(RejectsWith(V3("intimacy = 3"), "unknown tier '3' in [moments]"));
    const std::string feelingMsg = "feeling in [moments] must be warm, treasured or reserved";
    CHECK(RejectsWith(V3("feeling = \"Warm\""), feelingMsg));
    CHECK(RejectsWith(V3("feeling = \"cold\""), feelingMsg));
    CHECK(RejectsWith(V3("feeling = 1"), feelingMsg));
    CHECK(RejectsWith(V3("closenes = \"Friend\""), "unknown field 'closenes'"));
    const auto notTable = "schema_version = 3\nmoments = 1\n[npc]\nplugin = \"ExampleFollower.esp\"\nform_id = \"0x000800\"\n[romance]\nlikes = [\"Barters\"]\n";
    CHECK(RejectsWith(notTable, "[moments] must be a table"));
}

TEST(profile_v1_and_v2_reject_moments_as_unknown)
{
    CHECK(RejectsWith(valid + "[moments]\naffection = \"Friend\"\n", "unknown field 'moments'"));
    CHECK(RejectsWith(V2("likes = [\"Barters\"]", "[moments]\naffection = \"Friend\"\n"), "unknown field 'moments'"));
}

TEST(player_profile_rejects_moments)
{
    CHECK(RejectsWith(V3("affection = \"Friend\""), "[moments] is not allowed in a player profile", true));
    // The player refusal comes before any error inside the table.
    CHECK(RejectsWith(V3("affection = \"Sweetheart\""), "[moments] is not allowed in a player profile", true));
    CHECK(RejectsWith(V3("affection = \"Sweetheart\""), "Sweetheart"));
    // A version 3 player file without the table is fine.
    CHECK(ParseProfile(Replace(valid, "schema_version = 1", "schema_version = 3"), "test.toml", true).playerOwned);
}

TEST(player_profile_writes_v2_only_when_needed)
{
    const auto read = [](const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    };
    TempDirectory temp;
    auto profile = ParseProfile(valid, "", true);
    const auto plain = SavePlayerProfile(temp.path, profile);
    const auto plainText = read(plain.source);
    CHECK(plainText.find("schema_version = 1") != std::string::npos);
    CHECK(plainText.find("favorites") == std::string::npos);
    CHECK(plainText.find("pet_peeves") == std::string::npos);

    profile.preferences["ROM_Barters"] = 2;
    profile.preferences["ROM_Murders"] = -2;
    const auto strong = SavePlayerProfile(temp.path, profile);
    const auto strongText = read(strong.source);
    CHECK(strongText.find("schema_version = 2") != std::string::npos);
    CHECK(strongText.find("favorites") != std::string::npos);
    CHECK(strongText.find("pet_peeves") != std::string::npos);
    CHECK(strong.preferences == profile.preferences);
    const auto loaded = LoadProfiles(temp.path, true);
    CHECK(loaded.errors.empty());
    CHECK(loaded.profiles.size() == 1);
    CHECK(loaded.profiles.front().preferences == profile.preferences);

    auto bad = profile;
    bad.preferences["ROM_Bribes"] = 3;
    bool rejected = false;
    try { (void)SavePlayerProfile(temp.path, bad); } catch (const std::exception&) { rejected = true; }
    CHECK(rejected);
    auto withUnlocks = profile;
    withUnlocks.unlocks[2].push_back("x");
    rejected = false;
    try { (void)SavePlayerProfile(temp.path, withUnlocks); } catch (const std::exception&) { rejected = true; }
    CHECK(rejected);
}

TEST(check_preference_weights_rules)
{
    using Map = std::unordered_map<std::string, std::int32_t>;
    CHECK(!CheckPreferenceWeights(Map{}).has_value());
    CHECK(!CheckPreferenceWeights(Map{{"a", 2}, {"b", 2}, {"c", 2}, {"d", 1}}).has_value());
    CHECK(CheckPreferenceWeights(Map{{"a", 2}, {"b", 2}, {"c", 2}, {"d", 2}}).has_value());
    CHECK(CheckPreferenceWeights(Map{{"a", -2}, {"b", -2}, {"c", -2}, {"d", -2}}).has_value());
    CHECK(CheckPreferenceWeights(Map{{"a", 0}}).has_value());
    CHECK(CheckPreferenceWeights(Map{{"a", 3}}).has_value());
}

TEST(enforce_strong_cap_leaves_a_legal_map_alone)
{
    using Map = std::unordered_map<std::string, std::int32_t>;
    Map weights{{"a", 2}, {"b", 2}, {"c", 2}, {"d", -2}, {"e", -2}, {"f", -2}, {"g", 1}, {"h", -1}, {"i", 0}};
    const auto before = weights;
    CHECK(EnforceStrongCap(weights) == 0);
    CHECK(weights == before);
}

TEST(enforce_strong_cap_keeps_the_three_lowest_keys)
{
    using Map = std::unordered_map<std::string, std::int32_t>;
    Map weights{{"e", 2}, {"b", 2}, {"d", 2}, {"a", 2}, {"c", 2}};
    CHECK(EnforceStrongCap(weights) == 2);
    CHECK(weights.at("a") == 2);
    CHECK(weights.at("b") == 2);
    CHECK(weights.at("c") == 2);
    CHECK(weights.at("d") == 1);
    CHECK(weights.at("e") == 1);
}

TEST(enforce_strong_cap_caps_favorites_and_pet_peeves_separately)
{
    using Map = std::unordered_map<std::string, std::int32_t>;
    Map weights{{"a", 2}, {"b", 2}, {"c", 2}, {"d", 2}, {"p", -2}, {"q", -2}, {"r", -2}, {"s", -2}, {"t", -2}, {"x", 1}, {"y", -1}};
    CHECK(EnforceStrongCap(weights) == 3);
    CHECK(weights.at("d") == 1);
    CHECK(weights.at("p") == -2);
    CHECK(weights.at("r") == -2);
    CHECK(weights.at("s") == -1);
    CHECK(weights.at("t") == -1);
    CHECK(weights.at("x") == 1);
    CHECK(weights.at("y") == -1);
    CHECK(!CheckPreferenceWeights(weights).has_value());
}

TEST(preference_points_doubles_for_strong_weights)
{
    CHECK(PreferencePoints(3, 5, 1) == 15);
    CHECK(PreferencePoints(3, 5, 2) == 30);
    CHECK(PreferencePoints(3, 5, -1) == -15);
    CHECK(PreferencePoints(3, 5, -2) == -30);
    CHECK(PreferencePoints(3, 5, 0) == 0);
    CHECK(PreferencePoints(-3, 5, 2) == -30);
}

TEST(preference_points_clamps_instead_of_overflowing)
{
    constexpr auto big = std::numeric_limits<std::int32_t>::max();
    CHECK(PreferencePoints(big, big, 2) == big);
    CHECK(PreferencePoints(big, big, -2) == std::numeric_limits<std::int32_t>::min());
    CHECK(PreferencePoints(big, 1, 1) == big);
}

TEST(papyrus_direction_collapses_weights_to_a_sign)
{
    CHECK(PapyrusDirection(-2) == -1);
    CHECK(PapyrusDirection(-1) == -1);
    CHECK(PapyrusDirection(0) == 0);
    CHECK(PapyrusDirection(1) == 1);
    CHECK(PapyrusDirection(2) == 1);
}
