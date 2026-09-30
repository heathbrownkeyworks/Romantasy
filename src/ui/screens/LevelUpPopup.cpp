#include "ui/screens/LevelUpPopup.h"

#include "ui/screens/Ledger.h"  // FormatPoints
#include "ui/screens/ControllerHints.h"

#include <imgui.h>

#include <algorithm>
#include <string>

namespace romantasy::ui
{
    namespace
    {
        constexpr float kCardW = 560.0f, kCardH = 330.0f;
        constexpr float kCenterX = 800.0f, kCenterY = 450.0f;

        bool DismissPressed()
        {
            return ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
                ImGui::IsKeyPressed(ImGuiKey_Space, false) || ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
        }

        void DrawDiamond(ImDrawList* dl, ImVec2 center, float half, ImU32 color)
        {
            const ImVec2 points[4] = {
                ImVec2(center.x, center.y - half), ImVec2(center.x + half, center.y),
                ImVec2(center.x, center.y + half), ImVec2(center.x - half, center.y)
            };
            dl->AddConvexPolyFilled(points, 4, color);
        }
    }

    PopupResult DrawLevelUpPopup(const LevelChange& change, PopupViewState& view, const theme::Fonts& fonts, const theme::Canvas& c, float dt, bool interactive, bool dimBackground, bool controllerActive)
    {
        PopupResult result;
        view.time += dt;
        view.lifetime += dt;

        const float inP = anim::EaseOut(view.in.Advance(dt));
        if (!view.dismissing && ((interactive && DismissPressed()) || view.lifetime >= PopupAutoDismissSeconds)) {
            view.Dismiss();
        }
        float outP = 0.0f;
        if (view.dismissing) {
            outP = anim::EaseSoft(view.out.Advance(dt));
            result.finished = view.out.Done();
        }
        const float alpha = std::clamp(inP * (1.0f - outP), 0.0f, 1.0f);
        const float scale = 0.92f + 0.08f * inP;
        const auto A = [alpha](ImU32 color) { return theme::MulAlpha(color, alpha); };

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (dimBackground) {
            dl->AddRectFilled(c.Min(), c.Max(), A(theme::WithAlpha(theme::Obsidian, 0.55f)));
        }

        const ImU32 accent = change.isLoss ? theme::Rose : theme::Amber;
        const ImU32 accentSoft = change.isLoss ? theme::RoseSoft : theme::AmberLight;
        const float w = c.S(kCardW * scale), h = c.S(kCardH * scale);
        const ImVec2 center = c.P(kCenterX, kCenterY);
        const ImVec2 min(center.x - w * 0.5f, center.y - h * 0.5f);
        const ImVec2 max(center.x + w * 0.5f, center.y + h * 0.5f);
        const float s = c.scale * scale;

        theme::DrawGlow(dl, min, max, c.S(theme::RadiusXxl), A(theme::WithAlpha(accent, 0.35f)), 26.0f * s);
        theme::DrawPanel(dl, min, max, c.S(theme::RadiusXxl), A(theme::ObsidianLight), A(theme::WithAlpha(accent, 0.8f)), std::max(1.0f, 1.5f * s));

        theme::DrawTextAligned(dl, fonts.ceremonial, 13.0f * s, ImVec2(center.x, min.y + 26.0f * s), A(accent), change.isLoss ? "BOND WOUNDED" : "BOND FORGED", 0.5f);

        const float pulse = anim::Pulse(view.time, 1.2f);
        theme::DrawHeart(dl, ImVec2(center.x, min.y + 100.0f * s), (88.0f + 3.5f * pulse) * s, A(accent), change.isLoss);

        theme::DrawTextAligned(dl, fonts.ceremonial, 26.0f * s, ImVec2(center.x, min.y + 156.0f * s), A(theme::Fg1), change.followerName.c_str(), 0.5f);

        // "previous  ◆  new" tier line, previous muted, new in accent.
        const float prevSize = 22.0f * s, newSize = 30.0f * s;
        const ImVec2 prevExtent = theme::MeasureText(fonts.script, prevSize, change.previousLevelName.c_str());
        const ImVec2 newExtent = theme::MeasureText(fonts.script, newSize, change.levelName.c_str());
        const float gap = 22.0f * s;
        const float total = prevExtent.x + gap + newExtent.x;
        float x = center.x - total * 0.5f;
        const float lineY = min.y + 196.0f * s;
        theme::DrawText(dl, fonts.script, prevSize, ImVec2(x, lineY + (newSize - prevSize) * 0.6f), A(theme::Fg3), change.previousLevelName.c_str());
        x += prevExtent.x + gap;
        DrawDiamond(dl, ImVec2(x - gap * 0.5f, lineY + newSize * 0.55f), 4.0f * s, A(accentSoft));
        theme::DrawText(dl, fonts.script, newSize, ImVec2(x, lineY), A(accent), change.levelName.c_str());

        const std::string delta = (change.pointsDelta >= 0 ? "+" : "") + FormatPoints(change.pointsDelta) + " points";
        theme::DrawTextAligned(dl, fonts.bodySemi, 18.0f * s, ImVec2(center.x, min.y + 246.0f * s), A(accent), delta.c_str(), 0.5f);

        const std::string next = change.nextLevelName.empty() ? std::string("The bond is complete.") : ("Next: " + change.nextLevelName);
        theme::DrawTextAligned(dl, fonts.body, 12.0f * s, ImVec2(center.x, min.y + 276.0f * s), A(theme::Fg3), next.c_str(), 0.5f);
        theme::DrawTextAligned(dl, fonts.body, 11.0f * s, ImVec2(center.x, min.y + 302.0f * s), A(theme::Fg4), controllerActive ? "A / B   continue" : "Enter   continue", 0.5f);
        if (controllerActive && interactive) DrawControllerHints({ fonts, c, alpha }, ControllerHintScope::Popup);

        return result;
    }
}
