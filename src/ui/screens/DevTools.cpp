#include "ui/screens/DevTools.h"

#include <imgui.h>

namespace romantasy::ui
{
    namespace
    {
        constexpr const char* kNote = "Testing utilities. Point adjustments and simulated deeds affect only actively following companions.";

        constexpr DebugButton kButtons[DebugButtonCount] = {
            { "Bond +500",     widgets::ButtonKind::Primary,   500,  "",                      0, false },
            { "Bond -500",     widgets::ButtonKind::Danger,    -500, "",                      0, false },
            { "Side quest +1", widgets::ButtonKind::Secondary, 0,    "Side Quests Completed", 1, false },
            { "Murder +1",     widgets::ButtonKind::Danger,    0,    "Murders",               1, false },
            { "Refresh",       widgets::ButtonKind::Ghost,     0,    "",                      0, true },
        };
    }

    const DebugButton& DebugButtonAt(int index) noexcept
    {
        return kButtons[index < 0 ? 0 : (index >= DebugButtonCount ? DebugButtonCount - 1 : index)];
    }

    std::optional<DebugRequest> DebugRequestFor(int index)
    {
        if (index < 0 || index >= DebugButtonCount || kButtons[index].refresh) return std::nullopt;
        DebugRequest request;
        if (kButtons[index].points != 0) request.points = kButtons[index].points;
        else {
            request.stat = kButtons[index].stat;
            request.delta = kButtons[index].delta;
        }
        return request;
    }

    void DrawDevToolsPanel(const widgets::Ctx& c, const ShellLayout& layout, float top, bool interactive, LedgerActions& actions)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float x = layout.rightMin.x, w = layout.rightMax.x - layout.rightMin.x;
        const float pad = c.S(20.0f), gap = c.S(10.0f), buttonH = c.S(38.0f);
        const float noteH = widgets::WrappedHeight(c.fonts.body, c.S(11.0f), kNote, w - pad * 2.0f);
        const float buttonsTop = top + pad + c.S(22.0f) + c.S(8.0f) + noteH + c.S(14.0f);

        // Lay the buttons out first (wrapping), then paint the frame under them.
        ImVec2 positions[DebugButtonCount];
        float cx = x + pad, cy = buttonsTop;
        for (int i = 0; i < DebugButtonCount; ++i) {
            const float bw = widgets::ButtonWidth(c, kButtons[i].label, kButtons[i].kind);
            if (cx + bw > x + w - pad && cx > x + pad) {
                cx = x + pad;
                cy += buttonH + gap;
            }
            positions[i] = { cx, cy };
            cx += bw + gap;
        }
        const float bottom = cy + buttonH + pad;

        widgets::CardFrame(c, dl, { x, top }, { x + w, bottom });
        theme::DrawText(dl, c.fonts.bodySemi, c.S(11.0f), { x + pad, top + pad + c.S(4.0f) }, c.A(theme::Fg1), "Developer tools");
        widgets::LockPill(c, dl, { x + w - pad - widgets::LockPillWidth(c, "Unlocked"), top + pad }, "Unlocked");
        widgets::WrappedText(c, dl, c.fonts.body, c.S(11.0f), { x + pad, top + pad + c.S(22.0f) + c.S(8.0f) }, c.A(theme::Fg3), kNote, w - pad * 2.0f);
        for (int i = 0; i < DebugButtonCount; ++i) {
            ImGui::SetCursorScreenPos(positions[i]);
            ImGui::PushID(i);
            if (widgets::Button(c, "##debug", kButtons[i].label, kButtons[i].kind, interactive)) {
                if (kButtons[i].refresh) actions.refreshRequested = true;
                else actions.debug = DebugRequestFor(i);
            }
            ImGui::PopID();
        }
    }
}
