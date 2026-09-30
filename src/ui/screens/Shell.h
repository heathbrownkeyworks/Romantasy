#pragma once

#include "ui/screens/Widgets.h"

namespace romantasy::ui
{
    // Screen regions in pixels. The column split follows the real display
    // width; padding follows the shared canvas scale.
    struct ShellLayout
    {
        ImVec2 min, max;            // whole display
        ImVec2 leftMin, leftMax;    // company column content area
        ImVec2 seamMin, seamMax;    // 2-unit gold seam
        ImVec2 rightMin, rightMax;  // pane content area
        float pad = 0.0f;           // 32 units
    };

    [[nodiscard]] ShellLayout ComputeShellLayout(const theme::Canvas& canvas, float bottomInset = 0.0f) noexcept;
    void DrawShellBackdrop(const widgets::Ctx& c, ImDrawList* dl, const ShellLayout& layout);
    // Close glyph at the top-right; emits an item. Returns true when clicked.
    bool DrawShellClose(const widgets::Ctx& c, const ShellLayout& layout, bool interactive);
}
