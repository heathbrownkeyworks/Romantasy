#include "check.h"

#include "ui/screens/Ledger.h"
#include "ui/screens/Theme.h"

#include "headless.h"
#include "ui/screens/DevTools.h"
#include "ui/screens/Modals.h"
#include "ui/screens/SettingsPane.h"

using namespace romantasy::ui;

TEST(tier_progress_tracks_500_point_bands)
{
    CHECK_NEAR(TierProgress(0), 0.0f, 1e-5);
    CHECK_NEAR(TierProgress(250), 0.5f, 1e-5);
    CHECK_NEAR(TierProgress(1240), 0.48f, 1e-5);
    CHECK_NEAR(TierProgress(2499), 0.998f, 1e-5);
    CHECK_NEAR(TierProgress(2500), 1.0f, 1e-5);
    CHECK_NEAR(TierProgress(9000), 1.0f, 1e-5);
    CHECK_NEAR(TierProgress(-40), 0.0f, 1e-5);
}

TEST(format_points_inserts_thousands_separators)
{
    CHECK(FormatPoints(0) == "0");
    CHECK(FormatPoints(999) == "999");
    CHECK(FormatPoints(1240) == "1,240");
    CHECK(FormatPoints(1234567) == "1,234,567");
    CHECK(FormatPoints(-500) == "-500");
    CHECK(FormatPoints(-12345) == "-12,345");
}

TEST(mul_alpha_scales_existing_alpha)
{
    const ImU32 half = theme::MulAlpha(theme::Fg3, 0.5f);  // Fg3 alpha is 140
    CHECK(((half >> IM_COL32_A_SHIFT) & 0xFF) == 70);
    CHECK((half & 0x00FFFFFFu) == (theme::Fg3 & 0x00FFFFFFu));
    CHECK(theme::MulAlpha(theme::Amber, 1.0f) == theme::Amber);
}

TEST(ledger_view_open_starts_animations)
{
    LedgerViewState view;
    view.selected = 1;
    view.OnOpen();
    CHECK(view.appIn.Running());
    CHECK(view.focusPending);
    CHECK(view.lastSelected == -1);
    CHECK(view.selected == 1);
}

TEST(ledger_view_open_resets_pane_and_swap)
{
    LedgerViewState view;
    view.pane = LedgerPane::Settings;
    view.lastPane = LedgerPane::Settings;
    view.OnOpen();
    CHECK(view.pane == LedgerPane::Dossier);
    CHECK(view.lastPane == LedgerPane::Dossier);
    CHECK(!view.paneSwap.Running());
}

TEST(bond_modal_copy_describes_profile_actions)
{
    const ModalCopy reset = BondModalCopy(LedgerModal::ConfirmReset);
    CHECK(std::string(reset.eyebrow) == "Player-created bond");
    CHECK(std::string(reset.title) == "Reset this bond?");
    CHECK(std::string(reset.body) == "The personality remains. This companion's points and recent history reset in the current save only.");
    CHECK(std::string(reset.confirm) == "Reset bond");
    CHECK(reset.confirmKind == widgets::ButtonKind::Secondary);
    const ModalCopy remove = BondModalCopy(LedgerModal::ConfirmRemove);
    CHECK(std::string(remove.title) == "Remove this bond?");
    CHECK(std::string(remove.body) == "This removes the bond from all saves using this configuration. Personalities supplied by follower mods remain protected.");
    CHECK(std::string(remove.confirm) == "Remove bond");
    CHECK(remove.confirmKind == widgets::ButtonKind::Danger);
}

TEST(developer_password_check_is_exact)
{
    CHECK(CheckDeveloperPassword("darthmaul666"));
    CHECK(!CheckDeveloperPassword("darthmaul66"));
    CHECK(!CheckDeveloperPassword("Darthmaul666"));
    CHECK(!CheckDeveloperPassword("darthmaul666 "));
    CHECK(!CheckDeveloperPassword(""));
}

TEST(ledger_view_modal_open_close_and_reset)
{
    LedgerViewState view;
    view.OpenModal(LedgerModal::ConfirmRemove, "AD005901");
    CHECK(view.modal == LedgerModal::ConfirmRemove);
    CHECK(view.modalIdentity == "AD005901");
    CHECK(view.modalIn.Running());
    CHECK(view.modalFocusPending);
    CHECK(!view.modalClosing);
    view.CloseModal();
    CHECK(view.modalClosing);
    CHECK(view.modalOut.Running());
    view.CloseModal();  // idempotent while closing
    CHECK(view.modalClosing);
    view.notice.Arm(3);
    view.OnOpen();
    CHECK(view.modal == LedgerModal::None);
    CHECK(!view.modalClosing);
    CHECK(view.screen == LedgerScreen::Console);
    CHECK(!view.notice.armed);
    CHECK(view.password[0] == 0);
    CHECK(!view.passwordRejected);
}

TEST(ledger_escape_closes_an_open_modal_not_the_console)
{
    HeadlessFrame frame(1600.0f, 900.0f);
    const UiState state = FixtureState();
    LedgerViewState view;
    view.OnOpen();
    frame.Begin();
    DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();

    view.OpenModal(LedgerModal::ConfirmRemove, "000A2C8E");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    frame.Begin();
    const LedgerActions withModal = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();
    CHECK(!withModal.closeRequested);
    CHECK(!withModal.bond.has_value());
    CHECK(view.modalClosing);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);

    for (int i = 0; i < 12; ++i) {  // 0.19 s > the 0.15 s fade out
        frame.Begin();
        DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
    }
    CHECK(view.modal == LedgerModal::None);

    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    frame.Begin();
    const LedgerActions plain = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();
    CHECK(plain.closeRequested);
}

TEST(ledger_draws_each_modal_headless_at_two_sizes)
{
    const float sizes[2][2] = { { 1600.0f, 900.0f }, { 1280.0f, 720.0f } };
    const LedgerModal modals[3] = { LedgerModal::ConfirmReset, LedgerModal::ConfirmRemove, LedgerModal::Password };
    for (const auto& size : sizes) {
        for (const LedgerModal modal : modals) {
            HeadlessFrame frame(size[0], size[1]);
            const UiState state = FixtureState();
            LedgerViewState view;
            view.OnOpen();
            view.OpenModal(modal, "000A2C8E");
            for (int i = 0; i < 3; ++i) {
                frame.Begin();
                const LedgerActions actions = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
                frame.End();
                CHECK(!actions.closeRequested);
                CHECK(!view.modalClosing);
                CHECK(!actions.bond.has_value());
            }
            CHECK(view.modal == modal);
        }
    }
}

TEST(ledger_confirm_modal_emits_a_bond_request)
{
    // A mouse click is used rather than driving Tab-based nav focus: the
    // confirm/remove modals set no default nav focus (only the password
    // modal calls SetKeyboardFocusHere), so there is no InvisibleButton for
    // a headless test to poll "has nav focus" against without reaching into
    // ImGui's private NavId bookkeeping. The click position is computed with
    // the same layout math DrawModal/DrawFooter use (Modals.cpp), which is
    // exact once the fade-in tween has settled (scale == 1.0).
    struct Case { LedgerModal modal; BondRequest::Kind kind; };
    const Case cases[2] = {
        { LedgerModal::ConfirmRemove, BondRequest::Kind::Remove },
        { LedgerModal::ConfirmReset, BondRequest::Kind::Reset },
    };
    for (const Case& tc : cases) {
        HeadlessFrame frame(1600.0f, 900.0f);
        const UiState state = FixtureState();
        LedgerViewState view;
        view.OnOpen();
        frame.Begin();
        DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();

        view.OpenModal(tc.modal, "000A2C8E");
        for (int i = 0; i < 14; ++i) {  // 0.224 s > the 0.20 s fade-in duration
            frame.Begin();
            DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
            frame.End();
        }
        CHECK(!view.modalIn.Running());

        frame.Begin();
        ImGui::Begin("##measure", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
        const widgets::Ctx c{ frame.fonts, frame.canvas, 1.0f };
        const ModalCopy copy = BondModalCopy(tc.modal);
        const float pad = c.S(26.0f), cardW = c.S(448.0f), innerW = cardW - pad * 2.0f;
        const float bandH = c.S(14.0f) * 2.0f + c.S(38.0f);
        const float copyH = widgets::WrappedHeight(c.fonts.body, c.S(12.0f), copy.body, innerW);
        const float cardH = pad + c.S(22.0f + 30.0f) + copyH + c.S(20.0f) + bandH;
        const ImVec2 center(frame.canvas.display.x * 0.5f, frame.canvas.display.y * 0.5f);
        const ImVec2 min(center.x - cardW * 0.5f, center.y - cardH * 0.5f);
        const ImVec2 max(center.x + cardW * 0.5f, center.y + cardH * 0.5f);
        const float bandTop = min.y + pad + c.S(22.0f + 30.0f) + copyH + c.S(20.0f);
        const float bw = widgets::ButtonWidth(c, copy.confirm, copy.confirmKind);
        const float by = bandTop + c.S(14.0f);
        const ImVec2 confirmCenter{ max.x - c.S(22.0f) - bw * 0.5f, by + c.S(38.0f) * 0.5f };
        ImGui::End();
        frame.End();

        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(confirmCenter.x, confirmCenter.y);
        frame.Begin();
        DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        frame.Begin();
        DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        frame.Begin();
        const LedgerActions actions = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();

        CHECK(actions.bond.has_value());
        if (actions.bond.has_value()) {
            CHECK(actions.bond->kind == tc.kind);
            CHECK(actions.bond->identity == "000A2C8E");
        }
        CHECK(view.notice.armed);
        CHECK(view.modalClosing);
    }
}

TEST(ledger_settings_developer_row_opens_the_password_modal_and_panel_draws)
{
    HeadlessFrame frame(1600.0f, 900.0f);
    UiState state = FixtureState();
    LedgerViewState view;
    view.OnOpen();
    view.pane = LedgerPane::Settings;
    view.lastPane = LedgerPane::Settings;
    for (int i = 0; i < 3; ++i) {  // locked: five rows, no panel
        frame.Begin();
        const LedgerActions actions = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
        CHECK(!actions.developerTools.has_value());
        CHECK(!actions.debug.has_value());
    }
    state.developerToolsUnlocked = true;
    for (int i = 0; i < 3; ++i) {  // unlocked: the frame must still draw without a close request
        frame.Begin();
        const LedgerActions actions = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
        CHECK(!actions.closeRequested);
    }
    view.OpenModal(LedgerModal::Password, {});
    frame.Begin();
    DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();
    CHECK(view.modal == LedgerModal::Password);
    CHECK(!view.modalFocusPending);  // consumed by the first modal frame
}

namespace
{
    // Mirrors SettingsPane.cpp's DrawSettingsPane row placement exactly, so a
    // click lands on the same InvisibleButton rect the row drew.
    ImVec2 SettingsRowCenter(const widgets::Ctx& c, const ShellLayout& layout, int row)
    {
        const ImVec2 min = layout.rightMin, max = layout.rightMax;
        const float panelTop = min.y + c.S(84.0f);
        const float rowW = max.x - min.x - c.S(32.0f);
        const float rowH = c.S(76.0f);
        const float rowX = min.x + c.S(16.0f);
        const float rowY = panelTop + c.S(16.0f) + static_cast<float>(row) * rowH;
        return { rowX + rowW * 0.5f, rowY + rowH * 0.5f };
    }

    // Mirrors DevTools.cpp's DrawDevToolsPanel layout, including the wrap
    // check, exactly -- so a click lands on the same button the panel drew.
    void DevToolsButtonCenters(const widgets::Ctx& c, const ShellLayout& layout, ImVec2 (&centers)[DebugButtonCount])
    {
        // DevTools.cpp's kNote text (private to that translation unit).
        constexpr const char* kNote = "Testing utilities for tuning bond thresholds - hidden from normal play.";
        const float panelTop = layout.rightMin.y + c.S(84.0f);
        const float panelH = c.S(76.0f) * static_cast<float>(SettingsRowCount) + c.S(32.0f);
        const float top = panelTop + panelH + c.S(14.0f);
        const float x = layout.rightMin.x, w = layout.rightMax.x - layout.rightMin.x;
        const float pad = c.S(20.0f), gap = c.S(10.0f), buttonH = c.S(38.0f);
        const float noteH = widgets::WrappedHeight(c.fonts.body, c.S(11.0f), kNote, w - pad * 2.0f);
        const float buttonsTop = top + pad + c.S(22.0f) + c.S(8.0f) + noteH + c.S(14.0f);

        float cx = x + pad, cy = buttonsTop;
        for (int i = 0; i < DebugButtonCount; ++i) {
            const DebugButton& button = DebugButtonAt(i);
            const float bw = widgets::ButtonWidth(c, button.label, button.kind);
            if (cx + bw > x + w - pad && cx > x + pad) {
                cx = x + pad;
                cy += buttonH + gap;
            }
            centers[i] = { cx + bw * 0.5f, cy + buttonH * 0.5f };
            cx += bw + gap;
        }
    }

    // Same 3-frame hover/press/release sequence ledger_confirm_modal_emits_a_bond_request
    // uses: an InvisibleButton reports its click on the release frame.
    LedgerActions ClickAt(HeadlessFrame& frame, const UiState& state, LedgerViewState& view, ImVec2 center)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(center.x, center.y);
        frame.Begin();
        DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        frame.Begin();
        DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        frame.Begin();
        const LedgerActions actions = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
        return actions;
    }

    // Settles the app fade-in and any pane swap so pane content sits at its
    // resting position (no `risen` offset from DrawPaneContent's swap rise).
    void SettleLedger(HeadlessFrame& frame, const UiState& state, LedgerViewState& view)
    {
        for (int i = 0; i < 25; ++i) {
            frame.Begin();
            DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
            frame.End();
        }
    }
}

TEST(ledger_developer_row_click_opens_password_when_locked)
{
    HeadlessFrame frame(1600.0f, 900.0f);
    const UiState state = FixtureState();  // developerToolsUnlocked defaults false
    LedgerViewState view;
    view.OnOpen();
    view.pane = LedgerPane::Settings;
    view.lastPane = LedgerPane::Settings;
    SettleLedger(frame, state, view);

    const widgets::Ctx c{ frame.fonts, frame.canvas, 1.0f };
    const ShellLayout layout = ComputeShellLayout(frame.canvas);
    const LedgerActions actions = ClickAt(frame, state, view, SettingsRowCenter(c, layout, SettingsDeveloperRow));

    CHECK(view.modal == LedgerModal::Password);
    CHECK(!actions.developerTools.has_value());
    CHECK(!actions.settings.has_value());
}

TEST(ledger_developer_row_click_locks_when_unlocked)
{
    HeadlessFrame frame(1600.0f, 900.0f);
    UiState state = FixtureState();
    state.developerToolsUnlocked = true;
    LedgerViewState view;
    view.OnOpen();
    view.pane = LedgerPane::Settings;
    view.lastPane = LedgerPane::Settings;
    SettleLedger(frame, state, view);

    const widgets::Ctx c{ frame.fonts, frame.canvas, 1.0f };
    const ShellLayout layout = ComputeShellLayout(frame.canvas);
    const LedgerActions actions = ClickAt(frame, state, view, SettingsRowCenter(c, layout, SettingsDeveloperRow));

    CHECK(actions.developerTools.has_value() && *actions.developerTools == false);
    CHECK(view.modal == LedgerModal::None);
}

TEST(ledger_debug_buttons_emit_requests_and_refresh_arms_notice)
{
    HeadlessFrame frame(1600.0f, 900.0f);
    UiState state = FixtureState();
    state.developerToolsUnlocked = true;
    LedgerViewState view;
    view.OnOpen();
    view.pane = LedgerPane::Settings;
    view.lastPane = LedgerPane::Settings;
    SettleLedger(frame, state, view);

    const widgets::Ctx c{ frame.fonts, frame.canvas, 1.0f };
    const ShellLayout layout = ComputeShellLayout(frame.canvas);
    ImVec2 centers[DebugButtonCount];
    DevToolsButtonCenters(c, layout, centers);

    const LedgerActions bondActions = ClickAt(frame, state, view, centers[0]);  // "Bond +500"
    CHECK(bondActions.debug.has_value());
    if (bondActions.debug.has_value()) {
        CHECK(bondActions.debug->points.has_value() && *bondActions.debug->points == 500);
    }

    const LedgerActions refreshActions = ClickAt(frame, state, view, centers[4]);  // "Refresh"
    CHECK(refreshActions.refreshRequested);
    CHECK(view.notice.armed);
    CHECK(!refreshActions.debug.has_value());
}

TEST(ledger_accepted_password_emits_developer_tools)
{
    // If SetKeyboardFocusHere() (fired inside DrawModal on the first frame
    // after OpenModal) does not carry active-edit focus headlessly across the
    // settle frames below, this test would need a click on the field first;
    // it did not -- the typed characters land without one (see report).
    HeadlessFrame frame(1600.0f, 900.0f);
    const UiState state = FixtureState();
    LedgerViewState view;
    view.OnOpen();
    view.OpenModal(LedgerModal::Password, {});
    for (int i = 0; i < 14; ++i) {  // 0.224 s > the 0.20 s fade-in duration
        frame.Begin();
        DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
    }
    CHECK(!view.modalIn.Running());

    ImGuiIO& io = ImGui::GetIO();
    constexpr const char* kPassword = "darthmaul666";
    for (const char* p = kPassword; *p; ++p) io.AddInputCharacter(static_cast<unsigned int>(*p));
    frame.Begin();
    DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();
    CHECK(std::string(view.password) == kPassword);

    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame.Begin();
    const LedgerActions actions = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();
    io.AddKeyEvent(ImGuiKey_Enter, false);

    CHECK(actions.developerTools.has_value() && *actions.developerTools == true);
    CHECK(view.modalClosing);
}

TEST(ledger_escape_closes_the_password_modal_while_its_field_is_active)
{
    HeadlessFrame frame(1600.0f, 900.0f);
    const UiState state = FixtureState();
    LedgerViewState view;
    view.OnOpen();
    view.OpenModal(LedgerModal::Password, {});
    for (int i = 0; i < 14; ++i) {  // 0.224 s > the 0.20 s fade-in; the field takes
        frame.Begin();            // focus via SetKeyboardFocusHere on the first modal frame
        DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
    }
    CHECK(ImGui::GetIO().WantTextInput);

    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    frame.Begin();
    const LedgerActions actions = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);

    CHECK(view.modalClosing);
    CHECK(!actions.closeRequested);
}

TEST(ledger_follower_click_leaves_settings_even_for_the_selected_follower)
{
    for (int target = 0; target < 2; ++target) {
        HeadlessFrame frame(1600, 900);
        UiState state = FixtureState();
        auto second = state.followers.front();
        second.name = "Illia";
        second.referenceFormID = "0001A6A6";
        state.followers.push_back(second);
        LedgerViewState view;
        view.OnOpen();
        view.pane = view.lastPane = LedgerPane::Settings;
        SettleLedger(frame, state, view);
        CHECK(view.pane == LedgerPane::Settings);

        // Actual pointer activation of the roster, including the selected row.
        const auto layout = ComputeShellLayout(frame.canvas);
        const ImVec2 rowCenter{ layout.leftMin.x + 160, layout.leftMin.y + 120 + target * 104.0f + 46 };
        const auto actions = ClickAt(frame, state, view, rowCenter);
        CHECK(view.selected == target);
        CHECK(view.pane == LedgerPane::Dossier);
        CHECK(view.lastPane == LedgerPane::Dossier);
        CHECK(!actions.closeRequested);
    }
}
