#include "ui/screens/SettingsPane.h"

#include "ui/screens/DevTools.h"

#include <imgui.h>

static_assert(romantasy::ui::SettingsRowCount <= sizeof(romantasy::ui::LedgerViewState::knobs) / sizeof(float),
    "LedgerViewState::knobs must hold every settings row");

namespace romantasy::ui
{
    namespace
    {
        constexpr SettingsRowInfo kRows[SettingsRowCount] = {
            { widgets::Icon::Sparkle,   "Open with Favorites Menu",     "Off: open with Left Ctrl + R. On: cast the Romantasy power from your Favorites menu; the hotkey is disabled." },
            { widgets::Icon::HeartUp,   "\"Romance Deepened\" popups",  "Show the gold modal when a bond crosses a tier upward." },
            { widgets::Icon::HeartDown, "\"Romance Wounded\" popups",   "Show the rose modal when a bond slips below a tier." },
            { widgets::Icon::Eye,       "Show away companions",         "Keep bonds in the ledger even when not following." },
            { widgets::Icon::Gear,      "Developer tools",              "Reveal testing utilities. Enabling prompts for a password." },
        };
    }

    const SettingsRowInfo& SettingsRow(int index) noexcept
    {
        return kRows[index < 0 ? 0 : (index >= SettingsRowCount ? SettingsRowCount - 1 : index)];
    }

    bool SettingValue(const UiState& state, int index) noexcept
    {
        switch (index) {
        case 0: return state.openWithFavorites;
        case 1: return state.showGainModals;
        case 2: return state.showLossModals;
        case 3: return state.showAwayFollowers;
        default: return state.developerToolsUnlocked;
        }
    }

    SettingsChange ToggleSetting(const UiState& state, int index) noexcept
    {
        SettingsChange change;
        const bool next = !SettingValue(state, index);
        switch (index) {
        case 0: change.openWithFavorites = next; break;
        case 1: change.showGainModals = next; break;
        case 2: change.showLossModals = next; break;
        case 3: change.showAwayFollowers = next; break;
        default: break;  // the developer row is not a settings.json field
        }
        return change;
    }

    void DrawSettingsPane(const widgets::Ctx& c, const UiState& state, LedgerViewState& view, const ShellLayout& layout,
        bool interactive, float dt, LedgerActions& actions)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 min = layout.rightMin, max = layout.rightMax;
        widgets::Eyebrow(c, dl, { min.x, min.y + c.S(8.0f) }, "The scribe's quill", theme::Fg3);
        widgets::ScriptTitle(c, dl, { min.x, min.y + c.S(22.0f) }, "Sealed Page", 34.0f, theme::Fg1);

        if (!view.knobsPrimed) {
            for (int i = 0; i < SettingsRowCount; ++i) view.knobs[i] = SettingValue(state, i) ? 1.0f : 0.0f;
            view.knobsPrimed = true;
        }

        const float panelTop = min.y + c.S(84.0f);
        const float rowW = max.x - min.x - c.S(32.0f);
        const float panelH = c.S(76.0f) * static_cast<float>(SettingsRowCount) + c.S(32.0f);
        widgets::CardFrame(c, dl, { min.x, panelTop }, { max.x, panelTop + panelH });

        if (!interactive) ImGui::BeginDisabled();
        for (int i = 0; i < SettingsRowCount; ++i) {
            // ToggleRow's InvisibleButton resets CursorPos.x to the window's content-region
            // edge after each item (no SameLine() used), so the row's x must be re-asserted
            // every iteration -- setting it once before the loop only places row 0 correctly.
            ImGui::SetCursorScreenPos({ min.x + c.S(16.0f), panelTop + c.S(16.0f) + static_cast<float>(i) * c.S(76.0f) });
            ImGui::PushID(i);
            const SettingsRowInfo& row = SettingsRow(i);
            if (widgets::ToggleRow(c, "##setting", rowW, row.icon, row.title, row.detail, SettingValue(state, i), view.knobs[i], dt) && interactive) {
                if (i == SettingsDeveloperRow) {
                    // The knob only moves once the facade flips developerToolsUnlocked.
                    if (state.developerToolsUnlocked) actions.developerTools = false;
                    else view.OpenModal(LedgerModal::Password, {});
                } else {
                    actions.settings = ToggleSetting(state, i);
                }
            }
            ImGui::PopID();
        }
        if (state.developerToolsUnlocked) {
            DrawDevToolsPanel(c, layout, panelTop + panelH + c.S(14.0f), interactive, actions);
        }
        if (!interactive) ImGui::EndDisabled();
    }
}
