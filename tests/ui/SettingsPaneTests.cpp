#include "check.h"

#include "ui/screens/SettingsPane.h"

using namespace romantasy::ui;

TEST(settings_rows_carry_expected_copy_in_order)
{
    CHECK(SettingsRowCount == 5);
    CHECK(SettingsDeveloperRow == 4);
    CHECK(std::string(SettingsRow(0).title) == "Open with Favorites Menu");
    CHECK(std::string(SettingsRow(0).detail) == "Off: open with Left Ctrl + R. On: cast the Romantasy power from your Favorites menu; the hotkey is disabled.");
    CHECK(std::string(SettingsRow(1).title) == "\"Romance Deepened\" popups");
    CHECK(std::string(SettingsRow(1).detail) == "Show the gold modal when a bond crosses a tier upward.");
    CHECK(std::string(SettingsRow(2).title) == "\"Romance Wounded\" popups");
    CHECK(std::string(SettingsRow(2).detail) == "Show the rose modal when a bond slips below a tier.");
    CHECK(std::string(SettingsRow(3).title) == "Show away companions");
    CHECK(std::string(SettingsRow(3).detail) == "Keep bonds in the ledger even when not following.");
    CHECK(std::string(SettingsRow(4).title) == "Developer tools");
    CHECK(std::string(SettingsRow(4).detail) == "Reveal testing utilities. Enabling prompts for a password.");
    CHECK(SettingsRow(0).icon == widgets::Icon::Sparkle);
    CHECK(SettingsRow(3).icon == widgets::Icon::Eye);
    CHECK(SettingsRow(4).icon == widgets::Icon::Gear);
}

TEST(settings_values_and_toggles_map_to_state_fields)
{
    UiState state;
    state.openWithFavorites = false;
    state.showGainModals = true;
    state.showLossModals = false;
    state.showAwayFollowers = true;
    state.developerToolsUnlocked = true;
    CHECK(SettingValue(state, 0) == false);
    CHECK(SettingValue(state, 1) == true);
    CHECK(SettingValue(state, 2) == false);
    CHECK(SettingValue(state, 3) == true);
    CHECK(SettingValue(state, 4) == true);

    const SettingsChange c0 = ToggleSetting(state, 0);
    CHECK(c0.openWithFavorites.has_value() && *c0.openWithFavorites == true);
    CHECK(!c0.showGainModals && !c0.showLossModals && !c0.showAwayFollowers);
    const SettingsChange c2 = ToggleSetting(state, 2);
    CHECK(c2.showLossModals.has_value() && *c2.showLossModals == true);
    const SettingsChange c3 = ToggleSetting(state, 3);
    CHECK(c3.showAwayFollowers.has_value() && *c3.showAwayFollowers == false);
    // The developer row is not a settings.json field: it routes to the password modal / lock instead.
    const SettingsChange c4 = ToggleSetting(state, 4);
    CHECK(!c4.openWithFavorites && !c4.showGainModals && !c4.showLossModals && !c4.showAwayFollowers);
}
