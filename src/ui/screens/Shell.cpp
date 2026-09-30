#include "ui/screens/Shell.h"

#include <imgui.h>

namespace romantasy::ui
{
    ShellLayout ComputeShellLayout(const theme::Canvas& canvas, float bottomInset) noexcept
    {
        ShellLayout l;
        l.min = { 0.0f, 0.0f };
        l.max = canvas.display;
        l.max.y -= bottomInset;
        l.pad = canvas.S(32.0f);
        const float leftW = canvas.display.x * 0.38f;
        const float seamW = canvas.S(2.0f);
        l.seamMin = { leftW, 0.0f };
        l.seamMax = { leftW + seamW, l.max.y };
        l.leftMin = { l.pad, l.pad };
        l.leftMax = { leftW - l.pad, l.max.y - l.pad };
        l.rightMin = { l.seamMax.x + l.pad, l.pad };
        // The close glyph owns the right margin (24 inset + 40 diameter + 12 gap),
        // so pane content stops short of it instead of drawing underneath.
        l.rightMax = { canvas.display.x - canvas.S(24.0f) - canvas.S(40.0f) - canvas.S(12.0f), l.max.y - l.pad };
        return l;
    }

    void DrawShellBackdrop(const widgets::Ctx& c, ImDrawList* dl, const ShellLayout& layout)
    {
        widgets::GlowBackdrop(c, dl, layout.min, layout.max);
        dl->AddRectFilledMultiColor(layout.seamMin, { layout.seamMax.x, (layout.seamMin.y + layout.seamMax.y) * 0.5f },
            c.A(theme::WithAlpha(theme::Amber, 0.15f)), c.A(theme::WithAlpha(theme::Amber, 0.15f)),
            c.A(theme::Amber), c.A(theme::Amber));
        dl->AddRectFilledMultiColor({ layout.seamMin.x, (layout.seamMin.y + layout.seamMax.y) * 0.5f }, layout.seamMax,
            c.A(theme::Amber), c.A(theme::Amber),
            c.A(theme::WithAlpha(theme::Copper, 0.2f)), c.A(theme::WithAlpha(theme::Copper, 0.2f)));
    }

    bool DrawShellClose(const widgets::Ctx& c, const ShellLayout& layout, bool interactive)
    {
        const float size = c.S(40.0f);
        ImGui::SetCursorScreenPos({ layout.max.x - c.S(24.0f) - size, layout.min.y + c.S(24.0f) });
        return widgets::IconButton(c, "##close", widgets::Icon::Close, 40.0f, false, interactive, true);
    }
}
