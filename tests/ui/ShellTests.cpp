#include "check.h"

#include "ui/screens/Shell.h"

using namespace romantasy::ui;

TEST(shell_layout_splits_the_display_38_seam_62)
{
    const theme::Canvas canvas = theme::Canvas::Fit(ImVec2(1920.0f, 1080.0f));  // scale 1.2
    const ShellLayout layout = ComputeShellLayout(canvas);
    CHECK_NEAR(layout.min.x, 0.0f, 1e-5);
    CHECK_NEAR(layout.max.x, 1920.0f, 1e-5);
    CHECK_NEAR(layout.max.y, 1080.0f, 1e-5);
    CHECK_NEAR(layout.pad, 32.0f * 1.2f, 1e-4);
    CHECK_NEAR(layout.seamMin.x, 1920.0f * 0.38f, 1e-3);
    CHECK_NEAR(layout.seamMax.x - layout.seamMin.x, 2.0f * 1.2f, 1e-4);
    CHECK_NEAR(layout.leftMin.x, layout.pad, 1e-4);
    CHECK_NEAR(layout.leftMax.x, layout.seamMin.x - layout.pad, 1e-3);
    CHECK_NEAR(layout.rightMin.x, layout.seamMax.x + layout.pad, 1e-3);
    // The close glyph's column (24 inset + 40 diameter + 12 gap) is reserved, not `pad`.
    CHECK_NEAR(layout.rightMax.x, 1920.0f - (24.0f + 40.0f + 12.0f) * 1.2f, 1e-3);
    CHECK_NEAR(layout.leftMin.y, layout.pad, 1e-4);
    CHECK_NEAR(layout.rightMax.y, 1080.0f - layout.pad, 1e-3);
}

TEST(shell_layout_stretches_with_ultrawide_but_keeps_rem_scale)
{
    const theme::Canvas canvas = theme::Canvas::Fit(ImVec2(3440.0f, 1440.0f));  // scale 1.6 (height-bound)
    const ShellLayout layout = ComputeShellLayout(canvas);
    CHECK_NEAR(layout.seamMin.x, 3440.0f * 0.38f, 1e-2);
    CHECK_NEAR(layout.pad, 32.0f * 1.6f, 1e-4);
    CHECK_NEAR(layout.max.x, 3440.0f, 1e-5);
}
