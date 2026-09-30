#include "ui/screens/Notice.h"

#include <imgui.h>

#include <cctype>

namespace romantasy::ui
{
    namespace
    {
        std::string Lower(std::string_view text)
        {
            std::string out(text);
            for (auto& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return out;
        }
    }

    bool IsWarningMessage(std::string_view message)
    {
        const std::string lower = Lower(message);
        return lower.find("could not") != std::string::npos || lower.find("cannot") != std::string::npos ||
            lower.find("protected") != std::string::npos;
    }

    void UpdateNotice(NoticeState& n, const UiState& state, float dt)
    {
        using Phase = NoticeState::Phase;
        if (n.armed && state.revision != n.seenRevision) {
            n.seenRevision = state.revision;
            if (!state.message.empty()) {
                n.armed = false;
                n.text = state.message;
                n.warn = IsWarningMessage(state.message);
                n.shown = 0.0f;
                if (n.phase == Phase::Hidden || n.phase == Phase::Out) {
                    n.phase = Phase::In;
                    n.tween.Start(0.20f);
                } else {
                    n.phase = Phase::Hold;  // a new message restarts the hold
                }
            }
        }
        switch (n.phase) {
        case Phase::In:
            n.tween.Advance(dt);
            if (!n.tween.Running()) n.phase = Phase::Hold;
            break;
        case Phase::Hold:
            n.shown += dt;
            if (n.shown >= NoticeHoldSeconds) {
                n.phase = Phase::Out;
                n.tween.Start(0.20f);
            }
            break;
        case Phase::Out:
            n.tween.Advance(dt);
            if (!n.tween.Running()) {
                n.phase = Phase::Hidden;
                n.text.clear();
            }
            break;
        case Phase::Hidden:
            break;
        }
    }

    float NoticeProgress(const NoticeState& n) noexcept
    {
        using Phase = NoticeState::Phase;
        switch (n.phase) {
        case Phase::In: return anim::EaseSoft(n.tween.Progress());
        case Phase::Hold: return 1.0f;
        case Phase::Out: return 1.0f - anim::EaseSoft(n.tween.Progress());
        case Phase::Hidden: return 0.0f;
        }
        return 0.0f;
    }

    void DrawNotice(const widgets::Ctx& base, const NoticeState& n, const theme::Canvas& canvas)
    {
        const float p = NoticeProgress(n);
        if (p <= 0.0f || n.text.empty()) return;
        const widgets::Ctx c{ base.fonts, base.canvas, base.alpha * p };
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const float px = c.S(12.2f);
        const float padX = c.S(18.0f), padY = c.S(10.0f), dotD = c.S(8.0f), gap = c.S(9.0f);
        const std::string text = widgets::ClipText(c.fonts.body, px, n.text, c.S(576.0f) - padX * 2.0f - dotD - gap);
        const ImVec2 extent = theme::MeasureText(c.fonts.body, px, text.c_str());
        const float w = padX * 2.0f + dotD + gap + extent.x, h = padY * 2.0f + px;
        const float cx = canvas.display.x * 0.5f;
        const float bottom = canvas.display.y - c.S(24.0f) + (1.0f - p) * c.S(10.0f);  // slides up as it fades in
        const ImVec2 min(cx - w * 0.5f, bottom - h), max(cx + w * 0.5f, bottom);
        const float radius = c.S(10.0f);
        const ImU32 accent = n.warn ? theme::Rose : theme::Amber;
        theme::DrawGlow(dl, min, max, radius, c.A(theme::WithAlpha(theme::Obsidian, 0.5f)), c.S(10.0f));
        theme::DrawPanel(dl, min, max, radius, c.A(theme::WithAlpha(theme::ObsidianLight, 0.97f)), c.A(theme::WithAlpha(accent, n.warn ? 0.45f : 0.4f)), 1.0f);
        const ImVec2 dot(min.x + padX + dotD * 0.5f, (min.y + max.y) * 0.5f);
        theme::GlowCircle(dl, dot, dotD, accent, 0.6f * c.alpha, 16);
        dl->AddCircleFilled(dot, dotD * 0.5f, c.A(accent), 16);
        theme::DrawText(dl, c.fonts.body, px, { dot.x + dotD * 0.5f + gap, min.y + padY }, c.A(theme::Fg1), text.c_str());
    }
}
