#include "check.h"

#include "ui/screens/UiState.h"

#include <nlohmann/json.hpp>

using namespace romantasy::ui;

TEST(ui_state_parses_full_payload)
{
    const auto json = nlohmann::json::parse(R"({
        "message": "Romance records synchronized.",
        "showAwayFollowers": false,
        "romances": [{
            "name": "Ambellina", "points": 1240, "levelName": "Friend", "nextGoal": "Confidant at 1,500",
            "isFollowing": true, "role": "Necromancer", "profileOrigin": "Author-defined",
            "sourcePlugin": "CSV_Ambellina.esp", "profileFile": "Ambellina.toml", "baseFormID": "AD005900", "referenceFormID": "AD005901",
            "likes": ["Undead Killed", "Books Read"], "dislikes": ["Murders"],
            "recent": [{"label": "Undead Killed", "points": 12, "when": "Today"}]
        }]
    })");

    const UiState state = UiStateFromJson(json);
    CHECK(state.message == "Romance records synchronized.");
    CHECK(state.showAwayFollowers == false);
    CHECK(state.followers.size() == 1);
    const auto& row = state.followers.front();
    CHECK(row.name == "Ambellina");
    CHECK(row.points == 1240);
    CHECK(row.levelName == "Friend");
    CHECK(row.nextGoal == "Confidant at 1,500");
    CHECK(row.isFollowing == true);
    CHECK(row.role == "Necromancer");
    CHECK(row.profileOrigin == "Author-defined");
    CHECK(row.sourcePlugin == "CSV_Ambellina.esp");
    CHECK(row.profileFile == "Ambellina.toml");
    CHECK(row.baseFormID == "AD005900");
    CHECK(row.likes.size() == 2 && row.likes[1] == "Books Read");
    CHECK(row.dislikes.size() == 1 && row.dislikes[0] == "Murders");
    CHECK(row.recent.size() == 1 && row.recent[0].points == 12 && row.recent[0].when == "Today");
}

TEST(ui_state_defaults_when_keys_missing)
{
    const UiState state = UiStateFromJson(nlohmann::json::object());
    CHECK(state.message.empty());
    CHECK(state.showAwayFollowers == true);
    CHECK(state.followers.empty());
}

TEST(ui_state_ignores_wrong_types)
{
    const auto json = nlohmann::json::parse(R"({"romances": "nope", "showAwayFollowers": 7})");
    const UiState state = UiStateFromJson(json);
    CHECK(state.followers.empty());
    CHECK(state.showAwayFollowers == true);
}

TEST(ui_state_formats_numeric_form_ids_as_hex)
{
    const auto json = nlohmann::json::parse(R"({"romances": [{"name": "X", "baseFormID": 2902458624}]})");
    const UiState state = UiStateFromJson(json);
    CHECK(state.followers.size() == 1);
    CHECK(state.followers[0].baseFormID == "AD000100");
}

TEST(level_change_parses_gain)
{
    const auto json = nlohmann::json::parse(R"({
        "followerName": "Inalion", "changeDirection": "gain",
        "previousLevelName": "Acquaintance", "levelName": "Friend", "nextLevelName": "Confidant",
        "points": 1000, "pointsDelta": 500
    })");
    const LevelChange change = LevelChangeFromJson(json);
    CHECK(change.followerName == "Inalion");
    CHECK(change.isLoss == false);
    CHECK(change.previousLevelName == "Acquaintance");
    CHECK(change.levelName == "Friend");
    CHECK(change.nextLevelName == "Confidant");
    CHECK(change.points == 1000);
    CHECK(change.pointsDelta == 500);
}

TEST(level_change_parses_loss)
{
    const auto json = nlohmann::json::parse(R"({"changeDirection": "loss", "pointsDelta": -500})");
    const LevelChange change = LevelChangeFromJson(json);
    CHECK(change.isLoss == true);
    CHECK(change.pointsDelta == -500);
}

TEST(ui_state_parses_settings_and_profile_flags)
{
    const auto json = nlohmann::json::parse(R"({
        "openWithFavorites": true, "showGainModals": false, "showLossModals": true,
        "romances": [{"name": "Lydia", "personalityEditable": true, "removable": true}]
    })");
    const UiState state = UiStateFromJson(json);
    CHECK(state.openWithFavorites == true);
    CHECK(state.showGainModals == false);
    CHECK(state.showLossModals == true);
    CHECK(state.followers.size() == 1);
    CHECK(state.followers[0].personalityEditable == true);
    CHECK(state.followers[0].removable == true);
}

TEST(ui_state_settings_and_profile_flags_default)
{
    const UiState state = UiStateFromJson(nlohmann::json::parse(R"({"romances": [{"name": "X"}]})"));
    CHECK(state.openWithFavorites == false);
    CHECK(state.showGainModals == true);
    CHECK(state.showLossModals == true);
    CHECK(state.followers[0].personalityEditable == false);
    CHECK(state.followers[0].removable == false);
}

TEST(ui_state_parses_enrollment_candidates_and_preference_options)
{
    const auto json = nlohmann::json::parse(R"({
        "enrollmentCandidates": [
            {"name": "Illia", "role": "Imperial · Mage", "sourcePlugin": "Skyrim.esm", "referenceFormID": "0001A6A6", "baseFormID": "0001A6A5"},
            "nope",
            {"name": "Eola", "referenceFormID": 108806}
        ],
        "preferenceOptions": [
            {"editorID": "ROM_Murders", "label": "Murders", "category": "Crime & Transgression", "points": 5},
            {"label": "missing id"},
            {"editorID": "ROM_Bribes", "label": "Bribes", "category": "Influence", "points": "one"}
        ]
    })");
    const UiState state = UiStateFromJson(json);
    CHECK(state.candidates.size() == 2);
    CHECK(state.candidates[0].name == "Illia");
    CHECK(state.candidates[0].role == "Imperial \xC2\xB7 Mage");
    CHECK(state.candidates[0].sourcePlugin == "Skyrim.esm");
    CHECK(state.candidates[0].referenceFormID == "0001A6A6");
    CHECK(state.candidates[0].baseFormID == "0001A6A5");
    CHECK(state.candidates[1].name == "Eola");
    CHECK(state.candidates[1].referenceFormID == "0001A906");
    CHECK(state.candidates[1].baseFormID.empty());
    CHECK(state.preferenceOptions.size() == 2);
    CHECK(state.preferenceOptions[0].editorID == "ROM_Murders");
    CHECK(state.preferenceOptions[0].label == "Murders");
    CHECK(state.preferenceOptions[0].category == "Crime & Transgression");
    CHECK(state.preferenceOptions[0].points == 5);
    CHECK(state.preferenceOptions[1].editorID == "ROM_Bribes");
    CHECK(state.preferenceOptions[1].points == 0);
    CHECK(state.developerToolsUnlocked == false);
    CHECK(state.revision == 0);
}

TEST(ui_state_defaults_when_enrollment_arrays_missing_or_mistyped)
{
    const auto json = nlohmann::json::parse(R"({"enrollmentCandidates": 3, "preferenceOptions": {"a": 1}})");
    const UiState state = UiStateFromJson(json);
    CHECK(state.candidates.empty());
    CHECK(state.preferenceOptions.empty());
}

TEST(row_identity_prefers_reference_form_id)
{
    FollowerRow row;
    row.baseFormID = "AD005900";
    CHECK(RowIdentity(row) == "AD005900");
    row.referenceFormID = "00000000";
    CHECK(RowIdentity(row) == "AD005900");
    row.referenceFormID = "AD005901";
    CHECK(RowIdentity(row) == "AD005901");
}
