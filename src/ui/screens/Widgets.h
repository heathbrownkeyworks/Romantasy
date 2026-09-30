#pragma once

#include "ui/screens/Theme.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace romantasy::ui::widgets
{
    // Per-frame drawing context shared by every widget.
    struct Ctx
    {
        const theme::Fonts& fonts;
        const theme::Canvas& canvas;
        float alpha = 1.0f;

        [[nodiscard]] ImU32 A(ImU32 color) const noexcept { return theme::MulAlpha(color, alpha); }
        [[nodiscard]] float S(float units) const noexcept { return canvas.S(units); }
    };

    enum class Icon { Close, Gear, Refresh, Plus, Sparkle, HeartUp, HeartDown, Eye, Heart, Claw, Lock, Dot };
    enum class ButtonKind { Primary, Secondary, Ghost, Danger };

    // Pure helpers (unit tested).
    [[nodiscard]] std::string Uppercase(std::string_view text);
    [[nodiscard]] constexpr float PipFraction(int tierIndex) noexcept { return static_cast<float>(tierIndex) / 5.0f; }
    [[nodiscard]] float KnobPosition(float t) noexcept;
    // Returns `text`, or a prefix plus "..." that fits in maxWidth (never cuts a UTF-8 sequence).
    [[nodiscard]] std::string ClipText(ImFont* font, float size, const std::string& text, float maxWidth);

    // Draw-only primitives (no ImGui item).
    void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 center, float size, ImU32 color, float thickness);
    void TrackedText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, std::string_view text, float tracking);
    [[nodiscard]] ImVec2 MeasureTracked(ImFont* font, float size, std::string_view text, float tracking);
    void DashedRect(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, float dash, float gap, float thickness);
    // Wrapped paragraph helpers (AddText with wrap width). Return the height in pixels.
    [[nodiscard]] float WrappedHeight(ImFont* font, float size, const char* text, float wrapWidth);
    float WrappedText(const Ctx& c, ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text, float wrapWidth);

    // Uppercase, tracked label (Poppins semibold). `size` in canvas units.
    void Eyebrow(const Ctx& c, ImDrawList* dl, ImVec2 pos, std::string_view text, ImU32 color, float size = 10.0f);
    [[nodiscard]] float EyebrowWidth(const Ctx& c, std::string_view text, float size = 10.0f);
    void ScriptTitle(const Ctx& c, ImDrawList* dl, ImVec2 pos, const char* text, float size, ImU32 color);
    [[nodiscard]] float PillWidth(const Ctx& c, std::string_view text, bool dot);
    void Pill(const Ctx& c, ImDrawList* dl, ImVec2 pos, std::string_view text, ImU32 color, bool dot);  // 22 units tall
    void TierMeter(const Ctx& c, ImDrawList* dl, ImVec2 pos, float width, float thickness, std::int32_t points, bool showPips);
    void MedallionHeart(const Ctx& c, ImDrawList* dl, ImVec2 center, float diameter);
    void HeroHeart(const Ctx& c, ImDrawList* dl, ImVec2 center, float size, float time);
    void Wordmark(const Ctx& c, ImDrawList* dl, ImVec2 pos, float size);
    void SealBadge(const Ctx& c, ImDrawList* dl, ImVec2 center, float diameter, const char* label, ImU32 color);
    void GlowBackdrop(const Ctx& c, ImDrawList* dl, ImVec2 min, ImVec2 max);
    void CardFrame(const Ctx& c, ImDrawList* dl, ImVec2 min, ImVec2 max, float radius = theme::RadiusXl);
    void FocusRing(const Ctx& c, ImDrawList* dl, ImVec2 min, ImVec2 max, float radius);
    // Dashed empty-state card: bold title, wrapped copy. Returns the height consumed.
    float DashedNote(const Ctx& c, ImDrawList* dl, ImVec2 min, float width, const char* title, const char* copy);
    // Small amber pill with a lock icon ("Unlocked"). 22 units tall, draw-only.
    [[nodiscard]] float LockPillWidth(const Ctx& c, const char* text);
    void LockPill(const Ctx& c, ImDrawList* dl, ImVec2 pos, const char* text);

    // Widgets that emit an ImGui item at the current cursor and advance it.
    // `round` draws a circular chip (and focus ring) instead of a rounded panel.
    bool IconButton(const Ctx& c, const char* id, Icon icon, float size, bool active, bool enabled, bool round = false);
    bool DashedButton(const Ctx& c, const char* id, ImVec2 size, const char* title, const char* subtitle, bool enabled);
    // Returns true on the frame the row is clicked or activated. `knob` (0..1) animates toward `on` over 150 ms.
    bool ToggleRow(const Ctx& c, const char* id, float width, Icon icon, const char* title, const char* detail, bool on, float& knob, float dt);
    // Button: 10 x 16 padding, radius 8, Poppins 13 (600 for Primary), 38 tall. Returns true on click.
    [[nodiscard]] float ButtonWidth(const Ctx& c, const char* label, ButtonKind kind);
    bool Button(const Ctx& c, const char* id, const char* label, ButtonKind kind, bool enabled, float minWidth = 0.0f);
    // ImGui::InputText restyled (warm fill, amber border, focus halo, Fg4 placeholder).
    // Edits `buffer` live; returns true only when Enter was pressed inside the field.
    bool TextField(const Ctx& c, const char* id, char* buffer, std::size_t capacity, ImVec2 size, const char* placeholder, bool password, float fontSize = 14.0f);
    // Three-cell Like / Neutral / Dislike stamp (three items). `direction` is -1, 0 or 1.
    // Returns true on the frame it changed. Total height 34 (28 cells + 3 padding each side).
    [[nodiscard]] float StampGroupWidth(const Ctx& c, float cellWidth = 77.0f);
    bool StampGroup(const Ctx& c, const char* id, std::int8_t& direction, float cellWidth = 77.0f);
    // Candidate chooser card with a radio mark. `locked` draws the selected look with no item.
    inline constexpr float RadioCardHeight = 58.0f;
    bool RadioCard(const Ctx& c, const char* id, float width, const char* name, const char* meta, bool selected, bool locked);
    // Ghost "<- BACK TO ..." pill, 34 tall. Returns true on click.
    bool BackLink(const Ctx& c, const char* id, const char* label);
}
