#include "ui/screens/ControllerHints.h"

#include <imgui_internal.h>  // processed input events, including mouse/keyboard handoff

#include <algorithm>

namespace romantasy::ui
{
    void ControllerInput::UpdateFromBackend(bool trackMousePosition)
    {
        const ImGuiIO& io = ImGui::GetIO();
        for (const ImGuiInputEvent& event : ImGui::GetCurrentContext()->InputEventsTrail) {
            if (event.Type == ImGuiInputEventType_Key && event.Key.Down) {
                SetGamepad(event.Source == ImGuiInputSource_Gamepad);
            } else if (event.Type == ImGuiInputEventType_Text ||
                (event.Type == ImGuiInputEventType_MouseButton && event.MouseButton.Down) ||
                event.Type == ImGuiInputEventType_MouseWheel ||
                (trackMousePosition && event.Type == ImGuiInputEventType_MousePos &&
                    (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))) {
                SetGamepad(false);
            }
        }
        if (!(io.BackendFlags & ImGuiBackendFlags_HasGamepad)) SetGamepad(false);
    }

    bool ControllerInput::UsingGamepad() const
    {
        // Like Tailor, trust the engine's device events. The Win32 backend's
        // independent XInput discovery must not veto a controller Skyrim sees.
        return _gamepad.load(std::memory_order_relaxed);
    }

    std::span<const ControllerHint> ControllerHintsFor(ControllerHintScope scope)
    {
        // Match the native ImGui navigation bindings used by the screens.
        static constexpr ControllerHint console[] = { { "D-pad", "Navigate" }, { "A", "Select" }, { "B", "Close" }, { "LS", "Scroll" } };
        static constexpr ControllerHint enrollment[] = { { "D-pad", "Navigate" }, { "A", "Select" }, { "B", "Back" }, { "LS", "Scroll" } };
        static constexpr ControllerHint modal[] = { { "D-pad", "Navigate" }, { "A", "Select" }, { "B", "Back" } };
        static constexpr ControllerHint text[] = { { "Keyboard", "Type" }, { "B", "Leave field" } };
        static constexpr ControllerHint modalText[] = { { "Keyboard", "Type" }, { "B", "Back" } };
        static constexpr ControllerHint popup[] = { { "A / B", "Continue" } };
        switch (scope) {
        case ControllerHintScope::Enrollment: return enrollment;
        case ControllerHintScope::Modal: return modal;
        case ControllerHintScope::TextField: return text;
        case ControllerHintScope::ModalTextField: return modalText;
        case ControllerHintScope::Popup: return popup;
        default: return console;
        }
    }

    void DrawControllerHints(const widgets::Ctx& c, ControllerHintScope scope)
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const float top = c.canvas.display.y - c.S(ControllerHintsHeight);
        const auto hints = ControllerHintsFor(scope);
        const float px = c.S(16.0f), gap = c.S(8.0f), spacing = c.S(24.0f), keyH = c.S(28.0f);
        const auto keyWidth = [&](const ControllerHint& hint) {
            return std::max(keyH, theme::MeasureText(c.fonts.bodySemi, px, hint.key).x + c.S(12.0f));
        };
        const float titlePx = c.S(12.0f);
        const float titleWidth = c.S(42.0f) + theme::MeasureText(c.fonts.bodySemi, titlePx, "CONTROLLER").x;
        float width = titleWidth;
        for (const auto& hint : hints) width += keyWidth(hint) + gap + theme::MeasureText(c.fonts.body, px, hint.action).x + spacing;
        float x = (c.canvas.display.x - width) * 0.5f;
        const float y = top + (c.S(ControllerHintsHeight) - keyH) * 0.5f;
        // An inset control strip sits in its own reserved space below the page.
        // Tailor-style badges stay readable without a full-width toolbar.
        const ImVec2 min{ x - c.S(18.0f), y - c.S(7.0f) };
        const ImVec2 max{ x + width + c.S(18.0f), y + keyH + c.S(7.0f) };
        dl->AddRectFilled({ 0.0f, top }, c.canvas.display, c.A(theme::PageFloor));
        theme::DrawGlow(dl, min, max, c.S(10.0f), c.A(theme::WithAlpha(theme::Amber, 0.08f)), c.S(5.0f));
        theme::DrawPanel(dl, min, max, c.S(10.0f), c.A(theme::ObsidianLight), c.A(theme::WithAlpha(theme::Copper, 0.4f)));

        const ImVec2 center{ x + c.S(14.0f), y + keyH * 0.5f - c.S(2.0f) };
        const auto point = [&](float dx, float dy) { return ImVec2(center.x + c.S(dx), center.y + c.S(dy)); };
        const ImVec2 outline[] = { point(-9,-6), point(9,-6), point(14,8), point(9,10), point(4,5), point(-4,5), point(-9,10), point(-14,8) };
        dl->AddPolyline(outline, 8, c.A(theme::AmberLight), ImDrawFlags_Closed, c.S(1.5f));
        dl->AddLine(point(-9,0), point(-3,0), c.A(theme::AmberLight), c.S(1.5f));
        dl->AddLine(point(-6,-3), point(-6,3), c.A(theme::AmberLight), c.S(1.5f));
        dl->AddCircleFilled(point(6,-1), c.S(1.5f), c.A(theme::AmberLight));
        dl->AddCircleFilled(point(9,3), c.S(1.5f), c.A(theme::AmberLight));
        const ImVec2 titleSize = theme::MeasureText(c.fonts.bodySemi, titlePx, "CONTROLLER");
        theme::DrawText(dl, c.fonts.bodySemi, titlePx, { x + c.S(42.0f), y + (keyH - titleSize.y) * 0.5f }, c.A(theme::CopperPale), "CONTROLLER");
        x += titleWidth + spacing;
        for (const auto& hint : hints) {
            const float keyW = keyWidth(hint);
            theme::DrawPanel(dl, { x, y }, { x + keyW, y + keyH }, c.S(4.0f), c.A(theme::Surface), c.A(theme::WithAlpha(theme::Fg1, 0.25f)));
            const ImVec2 keyText = theme::MeasureText(c.fonts.bodySemi, px, hint.key);
            theme::DrawText(dl, c.fonts.bodySemi, px, { x + (keyW - keyText.x) * 0.5f, y + (keyH - keyText.y) * 0.5f }, c.A(theme::Fg1), hint.key);
            x += keyW + gap;
            const ImVec2 label = theme::MeasureText(c.fonts.body, px, hint.action);
            theme::DrawText(dl, c.fonts.body, px, { x, y + (keyH - label.y) * 0.5f }, c.A(theme::Fg2), hint.action);
            x += label.x + spacing;
        }
    }
}
