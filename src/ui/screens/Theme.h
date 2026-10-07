#pragma once

#include <imgui.h>

#include <string>

namespace romantasy::ui::theme
{
    // ForgeMaster Earth palette and typography shared by the native screens.
    inline constexpr ImU32 Obsidian        = IM_COL32(0x0a, 0x08, 0x07, 255);
    inline constexpr ImU32 ObsidianLight   = IM_COL32(0x12, 0x10, 0x0c, 255);
    inline constexpr ImU32 ObsidianLighter = IM_COL32(0x1c, 0x18, 0x13, 255);
    inline constexpr ImU32 Surface         = IM_COL32(0x2a, 0x21, 0x18, 255);
    inline constexpr ImU32 SurfaceLight    = IM_COL32(0x38, 0x2c, 0x1f, 255);
    inline constexpr ImU32 PageFloor       = IM_COL32(0x1a, 0x13, 0x0c, 255);
    inline constexpr ImU32 Amber           = IM_COL32(0xf5, 0x9e, 0x0b, 255);
    inline constexpr ImU32 AmberLight      = IM_COL32(0xfb, 0xbf, 0x24, 255);
    inline constexpr ImU32 AmberDark       = IM_COL32(0xd9, 0x77, 0x06, 255);
    inline constexpr ImU32 Copper          = IM_COL32(0xb8, 0x73, 0x33, 255);
    inline constexpr ImU32 CopperLight     = IM_COL32(0xd9, 0x95, 0x63, 255);
    inline constexpr ImU32 CopperPale        = IM_COL32(0xf5, 0xdc, 0xb6, 255);  // secondary button text
    inline constexpr ImU32 Fg1             = IM_COL32(0xf4, 0xeb, 0xd8, 255);
    inline constexpr ImU32 Fg2             = IM_COL32(0xb8, 0xa2, 0x85, 255);
    inline constexpr ImU32 Fg3             = IM_COL32(0xf4, 0xeb, 0xd8, 140);  // rgba(...,.55)
    inline constexpr ImU32 Fg4             = IM_COL32(0xf4, 0xeb, 0xd8, 102);  // rgba(...,.40)
    inline constexpr ImU32 Rose            = IM_COL32(0xf4, 0x3f, 0x5e, 255);
    inline constexpr ImU32 RoseSoft        = IM_COL32(0xfd, 0xa4, 0xaf, 255);
    inline constexpr ImU32 Emerald         = IM_COL32(0x34, 0xd3, 0x99, 255);

    inline constexpr float RadiusMd  = 8.0f;
    inline constexpr float RadiusLg  = 10.0f;
    inline constexpr float RadiusXl  = 14.0f;
    inline constexpr float RadiusXxl = 18.0f;

    [[nodiscard]] constexpr ImU32 WithAlpha(ImU32 color, float alpha) noexcept
    {
        const auto a = static_cast<ImU32>(alpha <= 0.0f ? 0.0f : (alpha >= 1.0f ? 255.0f : alpha * 255.0f + 0.5f));
        return (color & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
    }

    [[nodiscard]] constexpr ImU32 MulAlpha(ImU32 color, float factor) noexcept
    {
        const float current = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
        return WithAlpha(color, current * factor);
    }

    [[nodiscard]] constexpr ImU32 LerpColor(ImU32 a, ImU32 b, float t) noexcept
    {
        t = t <= 0.0f ? 0.0f : (t >= 1.0f ? 1.0f : t);
        auto channel = [t](ImU32 x, ImU32 y, int shift) -> ImU32 {
            const float fa = static_cast<float>((x >> shift) & 0xFF);
            const float fb = static_cast<float>((y >> shift) & 0xFF);
            return static_cast<ImU32>(fa + (fb - fa) * t + 0.5f) << shift;
        };
        return channel(a, b, IM_COL32_R_SHIFT) | channel(a, b, IM_COL32_G_SHIFT) |
               channel(a, b, IM_COL32_B_SHIFT) | channel(a, b, IM_COL32_A_SHIFT);
    }

    struct Fonts
    {
        ImFont* ceremonial = nullptr;  // Montserrat 900
        ImFont* bodyLight = nullptr;   // Poppins 300
        ImFont* body = nullptr;        // Poppins 400
        ImFont* bodyMedium = nullptr;  // Poppins 500
        ImFont* bodySemi = nullptr;    // Poppins 600
        ImFont* bodyBold = nullptr;    // Poppins 700
        ImFont* script = nullptr;      // MonteCarlo 400
        bool loaded = false;
    };

    // Registers the seven TTF files from fontDir with the atlas, each backed by
    // Windows' fonts for the letters it lacks (Cyrillic, Greek, Latin Extended,
    // Chinese, Japanese, Korean). Returns false (and leaves the missing slots
    // null) when any file cannot be opened; the atlas always ends up with at
    // least ImGui's default font so drawing never dereferences a null font.
    bool LoadFonts(ImGuiIO& io, const std::string& fontDir, Fonts& out);

    // 1600 x 900 virtual canvas, uniformly scaled and centered in the display.
    struct Canvas
    {
        static constexpr float Width = 1600.0f;
        static constexpr float Height = 900.0f;

        float scale = 1.0f;
        ImVec2 origin{ 0.0f, 0.0f };
        ImVec2 display{ Width, Height };

        [[nodiscard]] static Canvas Fit(ImVec2 displaySize) noexcept;
        [[nodiscard]] ImVec2 P(float x, float y) const noexcept { return { origin.x + x * scale, origin.y + y * scale }; }
        [[nodiscard]] float S(float v) const noexcept { return v * scale; }
        [[nodiscard]] ImVec2 Min() const noexcept { return origin; }
        [[nodiscard]] ImVec2 Max() const noexcept { return { origin.x + Width * scale, origin.y + Height * scale }; }
    };

    void DrawPanel(ImDrawList* drawList, ImVec2 min, ImVec2 max, float radius, ImU32 fill, ImU32 border, float borderThickness = 1.0f);
    // Soft outward glow: `layers` expanding rounded rects fading from `color`.
    void DrawGlow(ImDrawList* drawList, ImVec2 min, ImVec2 max, float radius, ImU32 color, float spread, int layers = 4);
    // Radial glow: nested triangle fans with per-vertex alpha, peakAlpha at the centre, 0 at the rim.
    void GlowCircle(ImDrawList* drawList, ImVec2 center, float radius, ImU32 color, float peakAlpha, int segments = 48);
    // Heart outline (same path as DrawHeart) stroked with `thickness`.
    void DrawHeartOutline(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness);

    inline constexpr int HeartPointCount = 48;
    // Fills outPoints[HeartPointCount] with a heart outline of the given width, centered at center.
    void BuildHeartPath(ImVec2 center, float size, ImVec2* outPoints) noexcept;
    // Gold (or any fill) heart; wounded = true adds a dark crack down the middle.
    void DrawHeart(ImDrawList* drawList, ImVec2 center, float size, ImU32 fill, bool wounded);

    // Text helpers that tolerate a null font (fall back to the current ImGui font).
    void DrawText(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text);
    // alignX: 0 = left edge at anchor, 0.5 = centered on anchor, 1 = right edge at anchor.
    void DrawTextAligned(ImDrawList* drawList, ImFont* font, float size, ImVec2 anchor, ImU32 color, const char* text, float alignX);
    [[nodiscard]] ImVec2 MeasureText(ImFont* font, float size, const char* text);
}
