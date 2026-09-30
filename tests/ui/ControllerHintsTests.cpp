#include "check.h"
#include "headless.h"

#include "ui/screens/ControllerHints.h"
#include "ui/screens/Enrollment.h"
#include "ui/screens/Ledger.h"
#include "ui/screens/LevelUpPopup.h"

using namespace romantasy::ui;

namespace
{
    void ConnectController()
    {
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    }

    void Tick(HeadlessFrame& frame, ControllerInput& input, bool trackMousePosition = true)
    {
        frame.Begin();
        input.UpdateFromBackend(trackMousePosition);
        frame.End();
    }
}

TEST(controller_legends_follow_input_and_remain_visible_after_button_release)
{
    HeadlessFrame frame(1600, 900);
    ControllerInput input;
    ConnectController();
    Tick(frame, input);
    CHECK(!input.UsingGamepad());  // connection alone is not controller use
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_GamepadDpadDown, true);
    Tick(frame, input);
    CHECK(input.UsingGamepad());
    io.AddKeyEvent(ImGuiKey_GamepadDpadDown, false);
    Tick(frame, input);
    Tick(frame, input);
    CHECK(input.UsingGamepad());
    io.AddKeyEvent(ImGuiKey_Z, true);
    Tick(frame, input);
    CHECK(!input.UsingGamepad());
    io.AddKeyEvent(ImGuiKey_Z, false);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, true, 0.8f);
    Tick(frame, input);
    CHECK(input.UsingGamepad());
}

TEST(controller_legends_hide_for_mouse_click_wheel_and_typing)
{
    HeadlessFrame frame(1600, 900);
    ControllerInput input;
    ConnectController();
    Tick(frame, input);
    ImGuiIO& io = ImGui::GetIO();
    input.SetGamepad(true);
    io.AddMouseButtonEvent(0, true);
    Tick(frame, input);
    CHECK(!input.UsingGamepad());
    io.AddMouseButtonEvent(0, false);
    Tick(frame, input);
    input.SetGamepad(true);
    io.AddMouseWheelEvent(0, 1);
    Tick(frame, input);
    CHECK(!input.UsingGamepad());
    input.SetGamepad(true);
    io.AddInputCharacter('a');
    Tick(frame, input);
    CHECK(!input.UsingGamepad());
}

TEST(controller_legends_distinguish_real_mouse_motion_from_skyrim_cursor_sync)
{
    HeadlessFrame frame(1600, 900);
    ControllerInput input;
    ConnectController();
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(100, 100);
    Tick(frame, input);
    input.SetGamepad(true);
    io.AddMousePosEvent(130, 100);
    Tick(frame, input, false);  // game host observes physical mouse moves in its sink
    CHECK(input.UsingGamepad());
    io.AddMousePosEvent(160, 100);
    Tick(frame, input);
    CHECK(!input.UsingGamepad());
}

TEST(controller_legends_clear_on_disconnect_and_require_use_after_reconnect)
{
    HeadlessFrame frame(1600, 900);
    ControllerInput input;
    ConnectController();
    input.SetGamepad(true);  // input remembered from gameplay / Favorites power
    Tick(frame, input);
    CHECK(input.UsingGamepad());
    ImGui::GetIO().BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    Tick(frame, input);
    CHECK(!input.UsingGamepad());
    ConnectController();
    Tick(frame, input);
    CHECK(!input.UsingGamepad());
}

TEST(skyrim_controller_activity_does_not_depend_on_win32_xinput_detection)
{
    HeadlessFrame frame(1600, 900);
    ControllerInput input;
    input.SetGamepad(true);  // Skyrim's input sink received a controller button
    // A controller recognized by Skyrim need not be seen by the separate
    // Win32 backend. Native legend visibility must not use that backend flag.
    ImGui::GetIO().BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    frame.Begin();
    CHECK(input.UsingGamepad());
    frame.End();
    input.SetGamepad(false); // physical mouse/keyboard or engine disconnect event
    CHECK(!input.UsingGamepad());
}

TEST(controller_footer_reserves_content_space_at_standard_and_ultrawide_sizes)
{
    for (const ImVec2 size : { ImVec2(1280, 720), ImVec2(1920, 1080), ImVec2(3440, 1440) }) {
        const auto canvas = theme::Canvas::Fit(size);
        const float footer = canvas.S(ControllerHintsHeight);
        const auto layout = ComputeShellLayout(canvas, footer);
        CHECK_NEAR(layout.max.y, size.y - footer, 0.001f);
        CHECK(layout.leftMax.y < layout.max.y);
        CHECK(layout.rightMax.y < layout.max.y);
        CHECK_NEAR(layout.seamMax.y, layout.max.y, 0.001f);
    }
}

TEST(controller_back_with_legends_closes_only_the_active_layer)
{
    for (int layer = 0; layer < 3; ++layer) {
        HeadlessFrame frame(1600, 900);
        ConnectController();
        const UiState state = FixtureState();
        LedgerViewState view;
        view.OnOpen();
        if (layer == 1) OpenEnrollment(state, view, EnrollMode::Create, {});
        if (layer == 2) view.OpenModal(LedgerModal::ConfirmRemove, "000A2C8E");
        for (int i = 0; i < 20; ++i) {
            frame.Begin();
            DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true, true);
            frame.End();
        }
        ImGui::GetIO().AddKeyEvent(ImGuiKey_GamepadFaceRight, true);
        frame.Begin();
        const auto actions = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true, true);
        frame.End();
        CHECK(actions.closeRequested == (layer == 0));
        CHECK(!actions.bond.has_value());
        if (layer == 1) CHECK(view.enroll.closing);
        if (layer == 2) CHECK(view.modalClosing);
    }
}

TEST(controller_continue_dismisses_popup_without_activating_underlying_ledger)
{
    for (const ImGuiKey key : { ImGuiKey_GamepadFaceDown, ImGuiKey_GamepadFaceRight }) {
        HeadlessFrame frame(1600, 900);
        ConnectController();
        const UiState state = FixtureState();
        LedgerViewState ledger;
        ledger.OnOpen();
        PopupViewState popup;
        popup.Reset();
        LevelChange change;
        change.followerName = "Lydia";
        frame.Begin();
        DrawLedger(state, ledger, frame.fonts, frame.canvas, 0.016f, false, true);
        DrawLevelUpPopup(change, popup, frame.fonts, frame.canvas, 0.016f, true, true, true);
        frame.End();
        ImGui::GetIO().AddKeyEvent(key, true);
        frame.Begin();
        const auto actions = DrawLedger(state, ledger, frame.fonts, frame.canvas, 0.016f, false, true);
        DrawLevelUpPopup(change, popup, frame.fonts, frame.canvas, 0.016f, true, true, true);
        frame.End();
        CHECK(popup.dismissing);
        CHECK(!actions.closeRequested);
        CHECK(!actions.bond.has_value());
    }
}
