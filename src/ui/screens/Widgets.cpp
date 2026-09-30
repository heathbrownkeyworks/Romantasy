#include "ui/screens/Widgets.h"

#include "ui/screens/Anim.h"
#include "ui/screens/Tiers.h"

#include <imgui.h>
#include <imgui_internal.h>  // ImTextCharFromUtf8 is declared here in 1.92.6, not in imgui.h

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>

namespace romantasy::ui::widgets
{
    namespace
    {
        constexpr float kPi = 3.14159265f;

        ImFont* Resolve(ImFont* font) { return font ? font : ImGui::GetFont(); }

        void Star(ImDrawList* dl, ImVec2 center, float radius, ImU32 color)
        {
            const float inner = radius * 0.28f;
            const ImVec2 points[8] = {
                { center.x, center.y - radius }, { center.x + inner, center.y - inner },
                { center.x + radius, center.y }, { center.x + inner, center.y + inner },
                { center.x, center.y + radius }, { center.x - inner, center.y + inner },
                { center.x - radius, center.y }, { center.x - inner, center.y - inner },
            };
            dl->AddConcavePolyFilled(points, 8, color);
        }

        void Arrow(ImDrawList* dl, ImVec2 from, ImVec2 to, float head, ImU32 color, float thickness)
        {
            dl->AddLine(from, to, color, thickness);
            const float dx = to.x - from.x, dy = to.y - from.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len <= 0.0f) return;
            const float ux = dx / len, uy = dy / len;
            dl->AddLine(to, { to.x - head * (ux + uy * 0.7f), to.y - head * (uy - ux * 0.7f) }, color, thickness);
            dl->AddLine(to, { to.x - head * (ux - uy * 0.7f), to.y - head * (uy + ux * 0.7f) }, color, thickness);
        }

        // Lerp the RGB toward parchment while keeping the colour's own alpha (hover brighten).
        ImU32 Brighten(ImU32 color, float t)
        {
            const float a = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
            return theme::LerpColor(color, theme::WithAlpha(theme::Fg1, a), t);
        }

        // Rounded gold gradient: a flat Amber rounded rect, then the 135-degree
        // gradient clipped to the inner span so the corners stay rounded.
        void GoldFill(ImDrawList* dl, ImVec2 min, ImVec2 max, float radius, float alpha)
        {
            dl->AddRectFilled(min, max, theme::MulAlpha(theme::Amber, alpha), radius);
            dl->PushClipRect({ min.x + radius, min.y }, { max.x - radius, max.y }, true);
            dl->AddRectFilledMultiColor({ min.x + radius, min.y }, { max.x - radius, max.y },
                theme::MulAlpha(theme::AmberLight, alpha), theme::MulAlpha(theme::Amber, alpha),
                theme::MulAlpha(theme::AmberDark, alpha), theme::MulAlpha(theme::Amber, alpha));
            dl->PopClipRect();
        }
    }

    std::string Uppercase(std::string_view text)
    {
        std::string out(text);
        for (auto& ch : out) {
            const auto byte = static_cast<unsigned char>(ch);
            if (byte < 0x80) ch = static_cast<char>(std::toupper(byte));
        }
        return out;
    }

    float KnobPosition(float t) noexcept
    {
        return anim::EaseSoft(std::clamp(t, 0.0f, 1.0f));
    }

    ImVec2 MeasureTracked(ImFont* font, float size, std::string_view text, float tracking)
    {
        ImFont* f = Resolve(font);
        ImVec2 total(0.0f, 0.0f);
        int glyphs = 0;
        const char* p = text.data();
        const char* end = text.data() + text.size();
        while (p < end) {
            unsigned int codepoint = 0;
            const int consumed = ImTextCharFromUtf8(&codepoint, p, end);
            if (consumed <= 0) break;
            const ImVec2 extent = f->CalcTextSizeA(size, FLT_MAX, 0.0f, p, p + consumed);
            total.x += extent.x;
            total.y = std::max(total.y, extent.y);
            p += consumed;
            ++glyphs;
        }
        if (glyphs > 1) total.x += tracking * static_cast<float>(glyphs - 1);
        return total;
    }

    void TrackedText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, std::string_view text, float tracking)
    {
        ImFont* f = Resolve(font);
        const char* p = text.data();
        const char* end = text.data() + text.size();
        float x = pos.x;
        while (p < end) {
            unsigned int codepoint = 0;
            const int consumed = ImTextCharFromUtf8(&codepoint, p, end);
            if (consumed <= 0) break;
            dl->AddText(f, size, { x, pos.y }, color, p, p + consumed);
            x += f->CalcTextSizeA(size, FLT_MAX, 0.0f, p, p + consumed).x + tracking;
            p += consumed;
        }
    }

    std::string ClipText(ImFont* font, float size, const std::string& text, float maxWidth)
    {
        if (MeasureTracked(font, size, text, 0.0f).x <= maxWidth) return text;
        std::size_t n = text.size();
        while (n > 0) {
            --n;
            while (n > 0 && (static_cast<unsigned char>(text[n]) & 0xC0) == 0x80) --n;  // stay on a codepoint boundary
            const std::string candidate = text.substr(0, n) + "...";
            if (MeasureTracked(font, size, candidate, 0.0f).x <= maxWidth) return candidate;
        }
        return "...";
    }

    void DashedRect(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, float dash, float gap, float thickness)
    {
        auto edge = [&](ImVec2 a, ImVec2 b) {
            const float dx = b.x - a.x, dy = b.y - a.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len <= 0.0f) return;
            const float ux = dx / len, uy = dy / len;
            for (float t = 0.0f; t < len; t += dash + gap) {
                const float e = std::min(t + dash, len);
                dl->AddLine({ a.x + ux * t, a.y + uy * t }, { a.x + ux * e, a.y + uy * e }, color, thickness);
            }
        };
        edge(min, { max.x, min.y });
        edge({ max.x, min.y }, max);
        edge(max, { min.x, max.y });
        edge({ min.x, max.y }, min);
    }

    void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float size, ImU32 color, float thickness)
    {
        const float r = size * 0.5f;
        switch (icon) {
        case Icon::Close:
            dl->AddLine({ c.x - r, c.y - r }, { c.x + r, c.y + r }, color, thickness);
            dl->AddLine({ c.x + r, c.y - r }, { c.x - r, c.y + r }, color, thickness);
            break;
        case Icon::Gear:
            dl->AddCircle(c, r * 0.62f, color, 24, thickness);
            dl->AddCircle(c, r * 0.22f, color, 12, thickness);
            for (int i = 0; i < 8; ++i) {
                const float a = kPi * 2.0f * static_cast<float>(i) / 8.0f;
                dl->AddLine({ c.x + std::cos(a) * r * 0.62f, c.y + std::sin(a) * r * 0.62f },
                            { c.x + std::cos(a) * r, c.y + std::sin(a) * r }, color, thickness);
            }
            break;
        case Icon::Refresh:
            dl->PathArcTo(c, r * 0.8f, -kPi * 0.25f, kPi * 1.35f, 24);
            dl->PathStroke(color, ImDrawFlags_None, thickness);
            Arrow(dl, { c.x + r * 0.8f * std::cos(-kPi * 0.25f) - r * 0.3f, c.y + r * 0.8f * std::sin(-kPi * 0.25f) + r * 0.3f },
                  { c.x + r * 0.8f * std::cos(-kPi * 0.25f), c.y + r * 0.8f * std::sin(-kPi * 0.25f) }, r * 0.35f, color, thickness);
            break;
        case Icon::Plus:
            dl->AddLine({ c.x - r, c.y }, { c.x + r, c.y }, color, thickness);
            dl->AddLine({ c.x, c.y - r }, { c.x, c.y + r }, color, thickness);
            break;
        case Icon::Sparkle:
            Star(dl, c, r, color);
            break;
        case Icon::HeartUp:
        case Icon::HeartDown: {
            // Small filled heart (a thin stroked outline blurred into an illegible
            // squiggle at 16-18px) beside a clearly separated vertical arrow.
            theme::DrawHeart(dl, { c.x - r * 0.42f, c.y + r * 0.05f }, size * 0.5f, color, false);
            const float ax = c.x + r * 0.62f;
            const float top = c.y - r * 0.85f, bottom = c.y + r * 0.65f;
            if (icon == Icon::HeartUp) Arrow(dl, { ax, bottom }, { ax, top }, r * 0.3f, color, thickness);
            else Arrow(dl, { ax, top }, { ax, bottom }, r * 0.3f, color, thickness);
            break;
        }
        case Icon::Eye:
            dl->PathArcTo({ c.x, c.y + r * 0.9f }, r * 1.25f, -kPi * 0.72f, -kPi * 0.28f, 16);
            dl->PathStroke(color, ImDrawFlags_None, thickness);
            dl->PathArcTo({ c.x, c.y - r * 0.9f }, r * 1.25f, kPi * 0.28f, kPi * 0.72f, 16);
            dl->PathStroke(color, ImDrawFlags_None, thickness);
            dl->AddCircle(c, r * 0.3f, color, 12, thickness);
            break;
        case Icon::Heart:
            theme::DrawHeart(dl, c, size, color, false);
            break;
        case Icon::Claw:
            // Three parallel diagonal slashes read as a claw mark; two thin lines
            // at 16-18px looked like a stray tick, not a claw.
            for (int i = -1; i <= 1; ++i) {
                const float off = static_cast<float>(i) * r * 0.42f;
                dl->AddLine({ c.x - r * 0.5f + off, c.y - r * 0.8f }, { c.x + r * 0.5f + off, c.y + r * 0.8f }, color, thickness * (i == 0 ? 1.0f : 0.85f));
            }
            break;
        case Icon::Lock: {
            const float bw = r * 1.3f, bh = r * 0.95f;
            const ImVec2 bmin(c.x - bw * 0.5f, c.y - r * 0.05f), bmax(c.x + bw * 0.5f, c.y - r * 0.05f + bh);
            dl->AddRect(bmin, bmax, color, r * 0.18f, 0, thickness);
            dl->PathArcTo({ c.x, bmin.y }, r * 0.5f, kPi, 2.0f * kPi, 12);  // shackle: upper semicircle
            dl->PathStroke(color, ImDrawFlags_None, thickness);
            dl->AddCircleFilled({ c.x, (bmin.y + bmax.y) * 0.5f }, r * 0.14f, color, 8);
            break;
        }
        case Icon::Dot:
            dl->AddCircleFilled(c, r * 0.32f, color, 12);
            break;
        }
    }

    void Eyebrow(const Ctx& c, ImDrawList* dl, ImVec2 pos, std::string_view text, ImU32 color, float size)
    {
        const std::string upper = Uppercase(text);
        TrackedText(dl, c.fonts.bodySemi, c.S(size), pos, c.A(color), upper, c.S(size) * 0.18f);
    }

    float EyebrowWidth(const Ctx& c, std::string_view text, float size)
    {
        return MeasureTracked(c.fonts.bodySemi, c.S(size), Uppercase(text), c.S(size) * 0.18f).x;
    }

    void ScriptTitle(const Ctx& c, ImDrawList* dl, ImVec2 pos, const char* text, float size, ImU32 color)
    {
        theme::DrawText(dl, c.fonts.script, c.S(size), pos, c.A(color), text);
    }

    float PillWidth(const Ctx& c, std::string_view text, bool dot)
    {
        const float textW = MeasureTracked(c.fonts.bodySemi, c.S(9.0f), Uppercase(text), c.S(9.0f) * 0.1f).x;
        return c.S(20.0f) + textW + (dot ? c.S(12.0f) : 0.0f);
    }

    void Pill(const Ctx& c, ImDrawList* dl, ImVec2 pos, std::string_view text, ImU32 color, bool dot)
    {
        const float h = c.S(22.0f);
        const float w = PillWidth(c, text, dot);
        dl->AddRectFilled(pos, { pos.x + w, pos.y + h }, c.A(theme::WithAlpha(color, 0.10f)), h * 0.5f);
        dl->AddRect(pos, { pos.x + w, pos.y + h }, c.A(theme::WithAlpha(color, 0.55f)), h * 0.5f, 0, std::max(1.0f, c.S(1.0f)));
        float x = pos.x + c.S(10.0f);
        if (dot) {
            dl->AddCircleFilled({ x + c.S(3.0f), pos.y + h * 0.5f }, c.S(3.0f), c.A(color), 12);
            x += c.S(12.0f);
        }
        const std::string upper = Uppercase(text);
        const ImVec2 extent = MeasureTracked(c.fonts.bodySemi, c.S(9.0f), upper, c.S(9.0f) * 0.1f);
        TrackedText(dl, c.fonts.bodySemi, c.S(9.0f), { x, pos.y + (h - extent.y) * 0.5f }, c.A(color), upper, c.S(9.0f) * 0.1f);
    }

    void TierMeter(const Ctx& c, ImDrawList* dl, ImVec2 pos, float width, float thickness, std::int32_t points, bool showPips)
    {
        const float r = thickness * 0.5f;
        dl->AddRectFilled(pos, { pos.x + width, pos.y + thickness }, c.A(theme::WithAlpha(theme::Fg1, 0.08f)), r);
        const float fill = width * tiers::MeterFraction(points);
        if (fill > 0.0f) {
            dl->AddRectFilledMultiColor(pos, { pos.x + fill, pos.y + thickness },
                c.A(theme::AmberDark), c.A(theme::Amber), c.A(theme::Amber), c.A(theme::AmberDark));
        }
        if (!showPips) return;
        const float half = c.S(3.5f);
        for (int i = 0; i < tiers::Count; ++i) {
            const ImVec2 p(pos.x + half + (width - 2.0f * half) * PipFraction(i), pos.y + thickness * 0.5f);
            const ImVec2 diamond[4] = { { p.x, p.y - half }, { p.x + half, p.y }, { p.x, p.y + half }, { p.x - half, p.y } };
            const bool lit = points >= tiers::Thresholds[i];
            dl->AddConvexPolyFilled(diamond, 4, c.A(lit ? theme::Amber : theme::WithAlpha(theme::Fg1, 0.25f)));
        }
    }

    void MedallionHeart(const Ctx& c, ImDrawList* dl, ImVec2 center, float diameter)
    {
        dl->AddCircleFilled(center, diameter * 0.5f, c.A(theme::ObsidianLighter), 32);
        dl->AddCircle(center, diameter * 0.5f, c.A(theme::WithAlpha(theme::Amber, 0.55f)), 32, std::max(1.0f, c.S(1.5f)));
        theme::DrawHeart(dl, { center.x, center.y + diameter * 0.03f }, diameter * 0.42f, c.A(theme::Amber), false);
    }

    void HeroHeart(const Ctx& c, ImDrawList* dl, ImVec2 center, float size, float time)
    {
        theme::GlowCircle(dl, center, size * 0.85f, theme::Amber, 0.18f * c.alpha);
        theme::DrawHeart(dl, { center.x, center.y + c.S(3.0f) }, size, c.A(theme::WithAlpha(theme::AmberDark, 0.9f)), false);
        theme::DrawHeart(dl, center, size, c.A(theme::Amber), false);
        theme::DrawHeart(dl, { center.x - size * 0.08f, center.y - size * 0.04f }, size * 0.50f, c.A(theme::WithAlpha(theme::AmberLight, 0.55f)), false);
        // Kept clear of the cleft: at the old centre/radii it spilled past the left
        // lobe's inner edge and read as a grey fringe inside the notch.
        dl->AddEllipseFilled({ center.x - size * 0.22f, center.y - size * 0.12f }, { size * 0.13f, size * 0.08f }, c.A(theme::WithAlpha(theme::Fg1, 0.35f)), -0.5f, 24);
        theme::DrawHeartOutline(dl, center, size, c.A(theme::WithAlpha(theme::AmberLight, 0.6f)), std::max(1.0f, c.S(1.0f)));

        const ImVec2 offsets[3] = { { 0.55f, -0.45f }, { 0.62f, 0.05f }, { -0.60f, 0.50f } };
        const float phases[3] = { 0.0f, 0.55f, 1.1f };
        for (int i = 0; i < 3; ++i) {
            const float pulse = anim::Pulse(time + phases[i], 1.6f);
            const float radius = c.S(5.0f + 3.0f * pulse);
            Star(dl, { center.x + offsets[i].x * size, center.y + offsets[i].y * size }, radius, c.A(theme::WithAlpha(theme::AmberLight, 0.35f + 0.65f * pulse)));
        }
    }

    void Wordmark(const Ctx& c, ImDrawList* dl, ImVec2 pos, float size)
    {
        static constexpr const char* kWord = "ROMANTASY";
        ImFont* font = Resolve(c.fonts.ceremonial);
        const float px = c.S(size);
        const float tracking = px * 0.08f;
        float x = pos.x;
        for (int i = 0; kWord[i] != '\0'; ++i) {
            const float t = i <= 4 ? 0.0f : static_cast<float>(i - 4) / 4.0f;  // "ROMAN" parchment, "TASY" ramps to amber
            const char glyph[2] = { kWord[i], '\0' };
            dl->AddText(font, px, { x, pos.y }, c.A(theme::LerpColor(theme::Fg1, theme::Amber, t)), glyph);
            x += font->CalcTextSizeA(px, FLT_MAX, 0.0f, glyph).x + tracking;
        }
        theme::DrawHeart(dl, { x + px * 0.28f, pos.y + px * 0.34f }, px * 0.42f, c.A(theme::Amber), false);
    }

    void SealBadge(const Ctx& c, ImDrawList* dl, ImVec2 center, float diameter, const char* label, ImU32 color)
    {
        dl->AddCircleFilled(center, diameter * 0.5f, c.A(theme::Surface), 32);
        dl->AddCircle(center, diameter * 0.5f, c.A(theme::WithAlpha(color, 0.7f)), 32, std::max(1.0f, c.S(1.5f)));
        const float px = diameter * 0.34f;
        const ImVec2 extent = theme::MeasureText(c.fonts.bodyBold, px, label);
        theme::DrawText(dl, c.fonts.bodyBold, px, { center.x - extent.x * 0.5f, center.y - extent.y * 0.5f }, c.A(color), label);
    }

    void GlowBackdrop(const Ctx& c, ImDrawList* dl, ImVec2 min, ImVec2 max)
    {
        const float w = max.x - min.x, h = max.y - min.y;
        dl->AddRectFilled(min, max, c.A(theme::WithAlpha(theme::Obsidian, 0.94f)));
        theme::GlowCircle(dl, { min.x + w * 0.18f, min.y + h * 0.12f }, h * 0.55f, theme::Amber, 0.10f * c.alpha);
        theme::GlowCircle(dl, { max.x - w * 0.12f, max.y - h * 0.10f }, h * 0.50f, theme::Copper, 0.10f * c.alpha);
        const float inset = c.S(12.0f);
        dl->AddRect({ min.x + inset, min.y + inset }, { max.x - inset, max.y - inset }, c.A(theme::WithAlpha(theme::Fg1, 0.08f)), c.S(2.0f), 0, 1.0f);
    }

    void CardFrame(const Ctx& c, ImDrawList* dl, ImVec2 min, ImVec2 max, float radius)
    {
        theme::DrawPanel(dl, min, max, c.S(radius), c.A(theme::WithAlpha(theme::ObsidianLight, 0.72f)), c.A(theme::WithAlpha(theme::Fg1, 0.06f)), 1.0f);
    }

    void FocusRing(const Ctx& c, ImDrawList* dl, ImVec2 min, ImVec2 max, float radius)
    {
        const float pad = c.S(2.0f);
        dl->AddRect({ min.x - pad, min.y - pad }, { max.x + pad, max.y + pad }, c.A(theme::WithAlpha(theme::Amber, 0.9f)), radius + pad, 0, std::max(1.0f, c.S(1.5f)));
    }

    bool IconButton(const Ctx& c, const char* id, Icon icon, float size, bool active, bool enabled, bool round)
    {
        const float px = c.S(size);
        if (!enabled) ImGui::BeginDisabled();
        const bool pressed = ImGui::InvisibleButton(id, { px, px }, ImGuiButtonFlags_EnableNav);
        if (!enabled) ImGui::EndDisabled();
        const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        const bool hovered = enabled && ImGui::IsItemHovered();
        const bool focused = enabled && ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float alpha = enabled ? 1.0f : 0.4f;
        const ImU32 fill = active ? theme::WithAlpha(theme::Amber, 0.16f) : theme::WithAlpha(theme::ObsidianLight, 0.8f);
        const ImU32 border = active ? theme::WithAlpha(theme::Amber, 0.6f) : theme::WithAlpha(theme::Fg1, hovered ? 0.2f : 0.08f);
        const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
        if (round) {
            dl->AddCircleFilled(center, px * 0.5f, c.A(theme::MulAlpha(fill, alpha)), 32);
            dl->AddCircle(center, px * 0.5f, c.A(theme::MulAlpha(border, alpha)), 32, 1.0f);
        } else {
            theme::DrawPanel(dl, min, max, c.S(theme::RadiusLg), c.A(theme::MulAlpha(fill, alpha)), c.A(theme::MulAlpha(border, alpha)), 1.0f);
        }
        const ImU32 iconColor = active ? theme::Amber : (hovered ? theme::Fg1 : theme::Fg2);
        DrawIcon(dl, icon, center, px * 0.4f, c.A(theme::MulAlpha(iconColor, alpha)), std::max(1.0f, c.S(1.6f)));
        if (focused) {
            if (round) dl->AddCircle(center, px * 0.5f + c.S(2.0f), c.A(theme::WithAlpha(theme::Amber, 0.9f)), 32, std::max(1.0f, c.S(1.5f)));
            else FocusRing(c, dl, min, max, c.S(theme::RadiusLg));
        }
        return pressed && enabled;
    }

    bool DashedButton(const Ctx& c, const char* id, ImVec2 size, const char* title, const char* subtitle, bool enabled)
    {
        if (!enabled) ImGui::BeginDisabled();
        const bool pressed = ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
        if (!enabled) ImGui::EndDisabled();
        const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        const bool hovered = enabled && ImGui::IsItemHovered();
        const bool focused = enabled && ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float alpha = enabled ? 1.0f : 0.4f;
        dl->AddRectFilled(min, max, c.A(theme::MulAlpha(theme::WithAlpha(theme::Amber, hovered ? 0.08f : 0.04f), alpha)), c.S(theme::RadiusLg));
        DashedRect(dl, min, max, c.A(theme::MulAlpha(theme::WithAlpha(theme::Amber, 0.55f), alpha)), c.S(6.0f), c.S(4.0f), 1.0f);
        const ImVec2 circle(min.x + c.S(28.0f), (min.y + max.y) * 0.5f);
        dl->AddCircle(circle, c.S(12.0f), c.A(theme::MulAlpha(theme::WithAlpha(theme::Amber, 0.6f), alpha)), 24, 1.0f);
        DrawIcon(dl, Icon::Plus, circle, c.S(10.0f), c.A(theme::MulAlpha(theme::Amber, alpha)), std::max(1.0f, c.S(1.6f)));
        theme::DrawText(dl, c.fonts.bodySemi, c.S(14.0f), { min.x + c.S(52.0f), min.y + c.S(12.0f) }, c.A(theme::MulAlpha(theme::Fg1, alpha)), title);
        theme::DrawText(dl, c.fonts.body, c.S(11.0f), { min.x + c.S(52.0f), min.y + c.S(34.0f) }, c.A(theme::MulAlpha(theme::Fg3, alpha)), subtitle);
        if (focused) FocusRing(c, dl, min, max, c.S(theme::RadiusLg));
        return pressed && enabled;
    }

    bool ToggleRow(const Ctx& c, const char* id, float width, Icon icon, const char* title, const char* detail, bool on, float& knob, float dt)
    {
        const float h = c.S(76.0f);
        const bool pressed = ImGui::InvisibleButton(id, { width, h }, ImGuiButtonFlags_EnableNav);
        const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        const bool hovered = ImGui::IsItemHovered();
        const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        knob = std::clamp(knob + (on ? dt : -dt) / 0.15f, 0.0f, 1.0f);
        const float k = KnobPosition(knob);

        if (hovered) dl->AddRectFilled(min, max, c.A(theme::WithAlpha(theme::Fg1, 0.03f)), c.S(theme::RadiusMd));
        dl->AddLine({ min.x + c.S(16.0f), max.y }, { max.x - c.S(16.0f), max.y }, c.A(theme::WithAlpha(theme::Fg1, 0.06f)), 1.0f);

        const ImVec2 iconCenter(min.x + c.S(16.0f) + c.S(18.0f), (min.y + max.y) * 0.5f);
        dl->AddCircleFilled(iconCenter, c.S(18.0f), c.A(theme::ObsidianLighter), 32);
        dl->AddCircle(iconCenter, c.S(18.0f), c.A(theme::WithAlpha(theme::Fg1, 0.08f)), 32, 1.0f);
        DrawIcon(dl, icon, iconCenter, c.S(16.0f), c.A(theme::Fg2), std::max(1.0f, c.S(1.5f)));

        const float trackW = c.S(44.0f), trackH = c.S(24.0f);
        const ImVec2 trackMin(max.x - c.S(16.0f) - trackW, iconCenter.y - trackH * 0.5f);
        const float textX = min.x + c.S(64.0f);
        const float textW = trackMin.x - c.S(16.0f) - textX;
        theme::DrawText(dl, c.fonts.bodySemi, c.S(14.0f), { textX, min.y + c.S(18.0f) }, c.A(theme::Fg1), ClipText(c.fonts.bodySemi, c.S(14.0f), title, textW).c_str());
        theme::DrawText(dl, c.fonts.body, c.S(12.0f), { textX, min.y + c.S(40.0f) }, c.A(theme::Fg3), ClipText(c.fonts.body, c.S(12.0f), detail, textW).c_str());

        dl->AddRectFilled(trackMin, { trackMin.x + trackW, trackMin.y + trackH }, c.A(theme::LerpColor(theme::SurfaceLight, theme::Amber, k)), trackH * 0.5f);
        dl->AddCircleFilled({ trackMin.x + c.S(12.0f) + k * c.S(20.0f), trackMin.y + trackH * 0.5f }, c.S(9.0f), c.A(theme::Fg1), 24);
        if (focused) FocusRing(c, dl, min, max, c.S(theme::RadiusMd));
        return pressed;
    }

    float WrappedHeight(ImFont* font, float size, const char* text, float wrapWidth)
    {
        return Resolve(font)->CalcTextSizeA(size, FLT_MAX, wrapWidth, text).y;
    }

    float WrappedText(const Ctx&, ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text, float wrapWidth)
    {
        ImFont* f = Resolve(font);
        dl->AddText(f, size, pos, color, text, nullptr, wrapWidth);
        return f->CalcTextSizeA(size, FLT_MAX, wrapWidth, text).y;
    }

    float DashedNote(const Ctx& c, ImDrawList* dl, ImVec2 min, float width, const char* title, const char* copy)
    {
        const float padX = c.S(14.0f), padY = c.S(13.0f);
        const float inner = width - padX * 2.0f;
        const float titleH = c.S(12.2f) + c.S(4.0f);
        const float copyH = WrappedHeight(c.fonts.body, c.S(10.6f), copy, inner);
        const float h = padY * 2.0f + titleH + copyH;
        const ImVec2 max(min.x + width, min.y + h);
        DashedRect(dl, min, max, c.A(theme::WithAlpha(theme::Fg1, 0.16f)), c.S(5.0f), c.S(4.0f), 1.0f);
        theme::DrawText(dl, c.fonts.bodySemi, c.S(12.2f), { min.x + padX, min.y + padY }, c.A(theme::Fg1), title);
        WrappedText(c, dl, c.fonts.body, c.S(10.6f), { min.x + padX, min.y + padY + titleH }, c.A(theme::Fg3), copy, inner);
        return h;
    }

    float LockPillWidth(const Ctx& c, const char* text)
    {
        const float px = c.S(9.3f);
        return c.S(10.0f) * 2.0f + c.S(11.0f) + c.S(6.0f) + MeasureTracked(c.fonts.bodyBold, px, Uppercase(text), px * 0.1f).x;
    }

    void LockPill(const Ctx& c, ImDrawList* dl, ImVec2 pos, const char* text)
    {
        const float h = c.S(22.0f), w = LockPillWidth(c, text), px = c.S(9.3f);
        dl->AddRectFilled(pos, { pos.x + w, pos.y + h }, c.A(theme::WithAlpha(theme::Amber, 0.12f)), h * 0.5f);
        dl->AddRect(pos, { pos.x + w, pos.y + h }, c.A(theme::WithAlpha(theme::Amber, 0.35f)), h * 0.5f, 0, 1.0f);
        float x = pos.x + c.S(10.0f);
        DrawIcon(dl, Icon::Lock, { x + c.S(5.5f), pos.y + h * 0.5f }, c.S(11.0f), c.A(theme::Amber), std::max(1.0f, c.S(1.4f)));
        x += c.S(11.0f) + c.S(6.0f);
        const std::string upper = Uppercase(text);
        const ImVec2 extent = MeasureTracked(c.fonts.bodyBold, px, upper, px * 0.1f);
        TrackedText(dl, c.fonts.bodyBold, px, { x, pos.y + (h - extent.y) * 0.5f }, c.A(theme::Amber), upper, px * 0.1f);
    }

    float ButtonWidth(const Ctx& c, const char* label, ButtonKind kind)
    {
        ImFont* font = kind == ButtonKind::Primary ? c.fonts.bodySemi : c.fonts.bodyMedium;
        return theme::MeasureText(font, c.S(13.0f), label).x + c.S(32.0f);
    }

    bool Button(const Ctx& c, const char* id, const char* label, ButtonKind kind, bool enabled, float minWidth)
    {
        ImFont* font = kind == ButtonKind::Primary ? c.fonts.bodySemi : c.fonts.bodyMedium;
        const float px = c.S(13.0f);
        const float w = std::max(minWidth, ButtonWidth(c, label, kind));
        const float h = c.S(38.0f);
        if (!enabled) ImGui::BeginDisabled();
        const bool pressed = ImGui::InvisibleButton(id, { w, h }, ImGuiButtonFlags_EnableNav);
        if (!enabled) ImGui::EndDisabled();
        ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        const bool hovered = enabled && ImGui::IsItemHovered();
        const bool held = enabled && ImGui::IsItemActive();
        const bool focused = enabled && ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
        if (held) {  // .btn:active: translateY(1px) scale(0.97)
            const float dx = (max.x - min.x) * 0.015f, dy = (max.y - min.y) * 0.015f;
            min = { min.x + dx, min.y + dy + c.S(1.0f) };
            max = { max.x - dx, max.y - dy + c.S(1.0f) };
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float alpha = enabled ? 1.0f : 0.38f;
        const float radius = c.S(theme::RadiusMd);
        ImU32 fill = 0, border = 0, text = theme::Fg2;
        switch (kind) {
        case ButtonKind::Primary:   fill = theme::Amber; text = theme::Obsidian; break;
        case ButtonKind::Secondary: fill = theme::WithAlpha(theme::Copper, 0.16f); border = theme::WithAlpha(theme::Copper, 0.4f); text = theme::CopperPale; break;
        case ButtonKind::Danger:    fill = theme::WithAlpha(theme::Rose, 0.12f); border = theme::WithAlpha(theme::Rose, 0.3f); text = theme::RoseSoft; break;
        case ButtonKind::Ghost:     fill = theme::WithAlpha(theme::Fg1, 0.04f); border = theme::WithAlpha(theme::Fg1, 0.10f); text = theme::Fg2; break;
        }
        if (kind == ButtonKind::Primary) {
            if (enabled) theme::DrawGlow(dl, min, max, radius, c.A(theme::WithAlpha(theme::Amber, hovered ? 0.28f : 0.2f)), c.S(12.0f));
            GoldFill(dl, min, max, radius, c.alpha * alpha);
        } else {
            if (hovered) fill = Brighten(fill, 0.1f);
            dl->AddRectFilled(min, max, c.A(theme::MulAlpha(fill, alpha)), radius);
            if (border) dl->AddRect(min, max, c.A(theme::MulAlpha(border, alpha)), radius, 0, 1.0f);
        }
        const ImVec2 extent = theme::MeasureText(font, px, label);
        theme::DrawText(dl, font, px, { (min.x + max.x) * 0.5f - extent.x * 0.5f, (min.y + max.y) * 0.5f - extent.y * 0.5f }, c.A(theme::MulAlpha(text, alpha)), label);
        if (focused) FocusRing(c, dl, min, max, radius);
        return pressed && enabled;
    }

    bool TextField(const Ctx& c, const char* id, char* buffer, std::size_t capacity, ImVec2 size, const char* placeholder, bool password, float fontSize)
    {
        ImFont* font = Resolve(c.fonts.body);
        const float px = c.S(fontSize);
        const float padX = c.S(14.0f);
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + size.x, min.y + size.y);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(min, max, c.A(theme::WithAlpha(theme::Fg1, 0.04f)), c.S(9.0f));

        ImGui::PushFont(font, px);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { padX, std::max(0.0f, (size.y - px) * 0.5f) });
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, c.S(9.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, 0);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, 0);
        ImGui::PushStyleColor(ImGuiCol_Text, c.A(theme::Fg1));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, c.A(theme::WithAlpha(theme::Amber, 0.35f)));
        ImGui::PushStyleColor(ImGuiCol_NavCursor, 0);
        ImGui::SetNextItemWidth(size.x);
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
        if (password) flags |= ImGuiInputTextFlags_Password;
        const bool submitted = ImGui::InputText(id, buffer, capacity, flags);
        const bool active = ImGui::IsItemActive();
        const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(3);
        ImGui::PopFont();

        if (buffer[0] == 0) {
            theme::DrawText(dl, font, px, { min.x + padX, min.y + (size.y - px) * 0.5f }, c.A(theme::Fg4), placeholder);
        }
        if (active) {
            const float halo = c.S(3.0f);
            dl->AddRect({ min.x - halo, min.y - halo }, { max.x + halo, max.y + halo }, c.A(theme::WithAlpha(theme::Amber, 0.1f)), c.S(12.0f), 0, halo);
        }
        dl->AddRect(min, max, c.A(active ? theme::Amber : theme::WithAlpha(theme::Amber, 0.5f)), c.S(9.0f), 0, 1.0f);
        if (focused && !active) FocusRing(c, dl, min, max, c.S(9.0f));
        return submitted;
    }

    float StampGroupWidth(const Ctx& c, float cellWidth)
    {
        return c.S(cellWidth) * 3.0f + c.S(6.0f);
    }

    bool StampGroup(const Ctx& c, const char* id, std::int8_t& direction, float cellWidth)
    {
        static constexpr const char* kLabels[3] = { "LIKE", "NEUTRAL", "DISLIKE" };
        static constexpr std::int8_t kValues[3] = { 1, 0, -1 };
        const float cellW = c.S(cellWidth), cellH = c.S(28.0f), pad = c.S(3.0f);
        const ImVec2 groupMin = ImGui::GetCursorScreenPos();
        const ImVec2 groupMax(groupMin.x + cellW * 3.0f + pad * 2.0f, groupMin.y + cellH + pad * 2.0f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        theme::DrawPanel(dl, groupMin, groupMax, c.S(9.0f), c.A(theme::WithAlpha(theme::Obsidian, 0.45f)), c.A(theme::WithAlpha(theme::Fg1, 0.08f)), 1.0f);
        const float px = c.S(9.6f), tracking = px * 0.1f, iconSize = c.S(13.0f), gap = c.S(5.6f), radius = c.S(6.4f);
        bool changed = false;
        ImGui::PushID(id);
        for (int i = 0; i < 3; ++i) {
            ImGui::SetCursorScreenPos({ groupMin.x + pad + cellW * static_cast<float>(i), groupMin.y + pad });
            ImGui::PushID(i);
            const bool pressed = ImGui::InvisibleButton("##cell", { cellW, cellH }, ImGuiButtonFlags_EnableNav);
            ImGui::PopID();
            const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
            const bool on = direction == kValues[i];
            const bool hovered = ImGui::IsItemHovered();
            const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
            ImU32 text = theme::Fg3;
            if (on && i == 0) {
                GoldFill(dl, min, max, radius, c.alpha);
                text = theme::Obsidian;
            } else if (on && i == 1) {
                dl->AddRectFilled(min, max, c.A(theme::WithAlpha(theme::Fg1, 0.12f)), radius);
                text = theme::Fg1;
            } else if (on) {
                dl->AddRectFilled(min, max, c.A(theme::WithAlpha(theme::Rose, 0.2f)), radius);
                dl->AddRect(min, max, c.A(theme::WithAlpha(theme::Rose, 0.45f)), radius, 0, 1.0f);
                text = theme::RoseSoft;
            } else if (hovered) {
                dl->AddRectFilled(min, max, c.A(theme::WithAlpha(theme::Fg1, 0.06f)), radius);
                text = theme::Fg1;
            }
            const ImVec2 extent = MeasureTracked(c.fonts.bodyBold, px, kLabels[i], tracking);
            float x = (min.x + max.x) * 0.5f - (iconSize + gap + extent.x) * 0.5f;
            const float cy = (min.y + max.y) * 0.5f;
            const Icon icon = i == 0 ? Icon::Heart : (i == 1 ? Icon::Dot : Icon::Claw);
            DrawIcon(dl, icon, { x + iconSize * 0.5f, cy }, iconSize, c.A(text), std::max(1.0f, c.S(1.5f)));
            x += iconSize + gap;
            TrackedText(dl, c.fonts.bodyBold, px, { x, cy - extent.y * 0.5f }, c.A(text), kLabels[i], tracking);
            if (focused) FocusRing(c, dl, min, max, radius);
            if (pressed && !on) {
                direction = kValues[i];
                changed = true;
            }
        }
        ImGui::PopID();
        ImGui::SetCursorScreenPos({ groupMin.x, groupMax.y });
        return changed;
    }

    bool RadioCard(const Ctx& c, const char* id, float width, const char* name, const char* meta, bool selected, bool locked)
    {
        const float h = c.S(RadioCardHeight);
        bool pressed = false;
        ImVec2 min, max;
        if (locked) {
            min = ImGui::GetCursorScreenPos();
            max = { min.x + width, min.y + h };
            ImGui::Dummy({ width, h });
        } else {
            pressed = ImGui::InvisibleButton(id, { width, h }, ImGuiButtonFlags_EnableNav);
            min = ImGui::GetItemRectMin();
            max = ImGui::GetItemRectMax();
        }
        const bool hovered = !locked && ImGui::IsItemHovered();
        const bool focused = !locked && ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImU32 fill = selected ? theme::WithAlpha(theme::Amber, 0.10f) : (hovered ? theme::WithAlpha(theme::Amber, 0.06f) : theme::WithAlpha(theme::Obsidian, 0.35f));
        const ImU32 ring = selected ? theme::WithAlpha(theme::Amber, 0.5f) : (hovered ? theme::WithAlpha(theme::Amber, 0.32f) : theme::WithAlpha(theme::Fg1, 0.08f));
        theme::DrawPanel(dl, min, max, c.S(10.0f), c.A(fill), c.A(ring), 1.0f);
        const ImVec2 mark(min.x + c.S(13.0f) + c.S(7.5f), (min.y + max.y) * 0.5f);
        dl->AddCircle(mark, c.S(7.5f), c.A(selected ? theme::WithAlpha(theme::Amber, 0.85f) : theme::WithAlpha(theme::Fg1, 0.28f)), 24, 1.0f);
        if (selected) {
            theme::GlowCircle(dl, mark, c.S(7.0f), theme::Amber, 0.5f * c.alpha, 16);
            dl->AddCircleFilled(mark, c.S(3.5f), c.A(theme::Amber), 16);
        }
        const float textX = min.x + c.S(13.0f) + c.S(15.0f) + c.S(11.0f);
        const float textW = max.x - c.S(13.0f) - textX;
        theme::DrawText(dl, c.fonts.bodySemi, c.S(13.0f), { textX, min.y + c.S(12.0f) }, c.A(theme::Fg1), ClipText(c.fonts.bodySemi, c.S(13.0f), name, textW).c_str());
        theme::DrawText(dl, c.fonts.body, c.S(9.6f), { textX, min.y + c.S(32.0f) }, c.A(theme::Fg3), ClipText(c.fonts.body, c.S(9.6f), meta, textW).c_str());
        if (focused) FocusRing(c, dl, min, max, c.S(10.0f));
        return pressed;
    }

    bool BackLink(const Ctx& c, const char* id, const char* label)
    {
        const std::string upper = Uppercase(label);
        const float px = c.S(11.0f), tracking = px * 0.08f;
        const ImVec2 extent = MeasureTracked(c.fonts.bodySemi, px, upper, tracking);
        const float arrowW = c.S(14.0f), gap = c.S(7.0f), padX = c.S(14.0f);
        const float w = padX * 2.0f + arrowW + gap + extent.x, h = c.S(34.0f);
        const bool pressed = ImGui::InvisibleButton(id, { w, h }, ImGuiButtonFlags_EnableNav);
        const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        const bool hovered = ImGui::IsItemHovered();
        const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        theme::DrawPanel(dl, min, max, c.S(theme::RadiusMd), c.A(theme::WithAlpha(theme::Fg1, hovered ? 0.08f : 0.04f)), c.A(theme::WithAlpha(theme::Fg1, 0.10f)), 1.0f);
        const ImU32 color = hovered ? theme::Fg1 : theme::Fg2;
        const float cy = (min.y + max.y) * 0.5f;
        float x = min.x + padX;
        Arrow(dl, { x + arrowW, cy }, { x, cy }, c.S(4.0f), c.A(color), std::max(1.0f, c.S(1.6f)));
        x += arrowW + gap;
        TrackedText(dl, c.fonts.bodySemi, px, { x, cy - extent.y * 0.5f }, c.A(color), upper, tracking);
        if (focused) FocusRing(c, dl, min, max, c.S(theme::RadiusMd));
        return pressed;
    }
}
