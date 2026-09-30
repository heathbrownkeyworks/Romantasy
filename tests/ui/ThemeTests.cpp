#include "check.h"

#include "ui/screens/Theme.h"

#include <imgui.h>

using namespace romantasy::ui::theme;

TEST(canvas_fit_scales_to_the_limiting_axis)
{
    const Canvas full = Canvas::Fit(ImVec2(1920.0f, 1080.0f));
    CHECK_NEAR(full.scale, 1.2f, 1e-5);
    CHECK_NEAR(full.origin.x, 0.0f, 1e-3);
    CHECK_NEAR(full.origin.y, 0.0f, 1e-3);

    const Canvas wide = Canvas::Fit(ImVec2(2560.0f, 1080.0f));
    CHECK_NEAR(wide.scale, 1.2f, 1e-5);
    CHECK_NEAR(wide.origin.x, 320.0f, 1e-3);
    CHECK_NEAR(wide.origin.y, 0.0f, 1e-3);

    const Canvas tall = Canvas::Fit(ImVec2(1600.0f, 1000.0f));
    CHECK_NEAR(tall.scale, 1.0f, 1e-5);
    CHECK_NEAR(tall.origin.y, 50.0f, 1e-3);

    const Canvas small = Canvas::Fit(ImVec2(1280.0f, 720.0f));
    CHECK_NEAR(small.scale, 0.8f, 1e-5);
}

TEST(canvas_point_and_size_helpers_apply_scale_and_origin)
{
    const Canvas canvas = Canvas::Fit(ImVec2(2560.0f, 1080.0f));
    const ImVec2 p = canvas.P(100.0f, 50.0f);
    CHECK_NEAR(p.x, 320.0f + 120.0f, 1e-3);
    CHECK_NEAR(p.y, 60.0f, 1e-3);
    CHECK_NEAR(canvas.S(10.0f), 12.0f, 1e-5);
    CHECK_NEAR(canvas.Max().x, 320.0f + 1920.0f, 1e-3);
    CHECK_NEAR(canvas.Max().y, 1080.0f, 1e-3);
}

TEST(with_alpha_replaces_only_the_alpha_channel)
{
    const ImU32 half = WithAlpha(Amber, 0.5f);
    CHECK((half & 0x00FFFFFFu) == (Amber & 0x00FFFFFFu));
    CHECK(((half >> IM_COL32_A_SHIFT) & 0xFF) == 127 || ((half >> IM_COL32_A_SHIFT) & 0xFF) == 128);
}

TEST(heart_path_is_bounded_and_symmetric)
{
    ImVec2 points[HeartPointCount];
    const ImVec2 center(100.0f, 100.0f);
    const float size = 64.0f;
    BuildHeartPath(center, size, points);

    float minY = 1e9f;
    float maxY = -1e9f;
    for (const auto& point : points) {
        CHECK(point.x >= center.x - size * 0.55f && point.x <= center.x + size * 0.55f);
        CHECK(point.y >= center.y - size * 0.6f && point.y <= center.y + size * 0.6f);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }
    CHECK(minY < center.y);
    CHECK(maxY > center.y);

    for (int i = 1; i < HeartPointCount / 2; ++i) {
        const auto& left = points[i];
        const auto& right = points[HeartPointCount - i];
        CHECK_NEAR(left.x - center.x, -(right.x - center.x), 1e-3);
        CHECK_NEAR(left.y, right.y, 1e-3);
    }
}

TEST(load_fonts_reads_all_seven_families)
{
    ImGuiContext* context = ImGui::CreateContext();
    Fonts fonts;
    const bool ok = LoadFonts(ImGui::GetIO(), "assets/fonts", fonts);
    CHECK(ok);
    CHECK(fonts.loaded);
    CHECK(fonts.ceremonial != nullptr);
    CHECK(fonts.bodyLight != nullptr);
    CHECK(fonts.body != nullptr);
    CHECK(fonts.bodyMedium != nullptr);
    CHECK(fonts.bodySemi != nullptr);
    CHECK(fonts.bodyBold != nullptr);
    CHECK(fonts.script != nullptr);
    ImGui::DestroyContext(context);
}

TEST(load_fonts_falls_back_when_files_are_missing)
{
    ImGuiContext* context = ImGui::CreateContext();
    Fonts fonts;
    const bool ok = LoadFonts(ImGui::GetIO(), "tests/does-not-exist", fonts);
    CHECK(!ok);
    CHECK(!fonts.loaded);
    CHECK(fonts.ceremonial == nullptr);
    CHECK(ImGui::GetIO().Fonts->Fonts.Size >= 1);  // default font registered
    ImGui::DestroyContext(context);
}
