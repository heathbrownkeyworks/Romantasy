#pragma once

#include "ui/screens/Ledger.h"

namespace romantasy::ui
{
    inline constexpr int SettingsRowCount = 5;
    inline constexpr int SettingsDeveloperRow = 4;  // routes to the password modal / lock, not settings.json

    struct SettingsRowInfo
    {
        widgets::Icon icon;
        const char* title;
        const char* detail;
    };

    [[nodiscard]] const SettingsRowInfo& SettingsRow(int index) noexcept;
    [[nodiscard]] bool SettingValue(const UiState& state, int index) noexcept;
    [[nodiscard]] SettingsChange ToggleSetting(const UiState& state, int index) noexcept;

    void DrawSettingsPane(const widgets::Ctx& c, const UiState& state, LedgerViewState& view, const ShellLayout& layout,
        bool interactive, float dt, LedgerActions& actions);
}
