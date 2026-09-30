#include "ui/screens/Roster.h"

#include <imgui.h>

#include <algorithm>
#include <string>

namespace romantasy::ui
{
    namespace
    {
        constexpr float kRowHeight = 92.0f, kRowGap = 12.0f, kRowPad = 18.0f;
        constexpr float kHeaderHeight = 120.0f, kFooterHeight = 88.0f, kFooterButton = 56.0f;

        void DrawCounts(const widgets::Ctx& c, ImDrawList* dl, ImVec2 rightAnchor, const tiers::Counts& counts)
        {
            const std::string bonds = std::to_string(counts.bonds);
            const std::string following = std::to_string(counts.following);
            const float numSize = c.S(11.0f), labelSize = c.S(10.0f), tracking = labelSize * 0.18f;
            const std::string bondsLabel = " BONDS \xC2\xB7 ", followingLabel = " FOLLOWING";
            const float w1 = theme::MeasureText(c.fonts.bodyBold, numSize, bonds.c_str()).x;
            const float w2 = widgets::MeasureTracked(c.fonts.bodySemi, labelSize, bondsLabel, tracking).x;
            const float w3 = theme::MeasureText(c.fonts.bodyBold, numSize, following.c_str()).x;
            const float w4 = widgets::MeasureTracked(c.fonts.bodySemi, labelSize, followingLabel, tracking).x;
            float x = rightAnchor.x - (w1 + w2 + w3 + w4);
            const float y = rightAnchor.y;
            theme::DrawText(dl, c.fonts.bodyBold, numSize, { x, y - c.S(1.0f) }, c.A(theme::Fg1), bonds.c_str()); x += w1;
            widgets::TrackedText(dl, c.fonts.bodySemi, labelSize, { x, y }, c.A(theme::Fg3), bondsLabel, tracking); x += w2;
            theme::DrawText(dl, c.fonts.bodyBold, numSize, { x, y - c.S(1.0f) }, c.A(theme::Fg1), following.c_str()); x += w3;
            widgets::TrackedText(dl, c.fonts.bodySemi, labelSize, { x, y }, c.A(theme::Fg3), followingLabel, tracking);
        }

        void DrawRow(const widgets::Ctx& c, ImDrawList* dl, const FollowerRow& row, ImVec2 min, ImVec2 max, bool selected, bool focused)
        {
            const float pad = c.S(kRowPad);
            const ImU32 fill = selected ? theme::WithAlpha(theme::Amber, 0.06f) : theme::WithAlpha(theme::ObsidianLight, 0.85f);
            const ImU32 border = selected ? theme::WithAlpha(theme::Amber, 0.7f) : theme::WithAlpha(theme::Fg1, 0.06f);
            theme::DrawPanel(dl, min, max, c.S(theme::RadiusLg), c.A(fill), c.A(border), 1.0f);

            widgets::MedallionHeart(c, dl, { min.x + pad + c.S(18.0f), (min.y + max.y) * 0.5f }, c.S(36.0f));

            const float textX = min.x + c.S(72.0f);
            const float rightEdge = max.x - pad;
            widgets::Eyebrow(c, dl, { textX, min.y + c.S(16.0f) }, row.role.empty() ? "Companion" : row.role, theme::Fg3);

            const std::string tier = row.levelName.empty() ? tiers::Names[tiers::TierIndex(row.points)] : row.levelName;
            const ImVec2 tierExtent = theme::MeasureText(c.fonts.script, c.S(20.0f), tier.c_str());
            theme::DrawTextAligned(dl, c.fonts.script, c.S(20.0f), { rightEdge - c.S(8.0f), min.y + c.S(10.0f) }, c.A(theme::Amber), tier.c_str(), 1.0f);

            const float nameMax = std::max(0.0f, rightEdge - c.S(8.0f) - tierExtent.x - c.S(12.0f) - textX);
            theme::DrawText(dl, c.fonts.bodySemi, c.S(17.0f), { textX, min.y + c.S(30.0f) }, c.A(theme::Fg1),
                widgets::ClipText(c.fonts.bodySemi, c.S(17.0f), row.name, nameMax).c_str());

            const std::string points = FormatPoints(row.points);
            const std::int32_t next = tiers::NextThreshold(row.points);
            float px = rightEdge;
            if (next >= 0) {
                const std::string threshold = " / " + FormatPoints(next);
                const ImVec2 te = theme::MeasureText(c.fonts.body, c.S(12.0f), threshold.c_str());
                px -= te.x;
                theme::DrawText(dl, c.fonts.body, c.S(12.0f), { px, min.y + c.S(43.0f) }, c.A(theme::Fg3), threshold.c_str());
            }
            theme::DrawTextAligned(dl, c.fonts.bodyBold, c.S(13.0f), { px, min.y + c.S(42.0f) }, c.A(theme::Fg1), points.c_str(), 1.0f);

            const char* pillText = row.isFollowing ? "Following" : "Away";
            const ImU32 pillColor = row.isFollowing ? theme::Emerald : theme::Fg4;
            const float pillW = widgets::PillWidth(c, pillText, true);
            widgets::Pill(c, dl, { rightEdge - pillW, min.y + c.S(57.0f) }, pillText, pillColor, true);

            const float meterX = textX;
            const float meterW = (rightEdge - pillW - c.S(12.0f)) - meterX;
            widgets::TierMeter(c, dl, { meterX, min.y + c.S(66.0f) }, meterW, c.S(4.0f), row.points, true);

            if (focused) widgets::FocusRing(c, dl, min, max, c.S(theme::RadiusLg));
        }
    }

    RosterResult DrawRoster(const widgets::Ctx& c, const UiState& state, const std::vector<const FollowerRow*>& rows,
        LedgerViewState& view, const ShellLayout& layout, bool interactive, bool settingsActive)
    {
        RosterResult result;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 min = layout.leftMin, max = layout.leftMax;

        // Header
        widgets::Wordmark(c, dl, { min.x, min.y - c.S(8.0f) }, 34.0f);
        widgets::Eyebrow(c, dl, { min.x, min.y + c.S(52.0f) }, "Those who walk beside you", theme::Fg3);
        widgets::ScriptTitle(c, dl, { min.x, min.y + c.S(64.0f) }, "Your Company", 34.0f, theme::Fg1);
        DrawCounts(c, dl, { max.x, min.y + c.S(86.0f) }, tiers::CountBonds(state, state.showAwayFollowers));

        // Rows
        const float listTop = min.y + c.S(kHeaderHeight);
        const float listBottom = max.y - c.S(kFooterHeight);
        ImGui::SetCursorScreenPos({ min.x, listTop });
        ImGui::BeginChild("##roster", { max.x - min.x, listBottom - listTop }, ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
        ImDrawList* listDl = ImGui::GetWindowDrawList();
        const float rowW = max.x - min.x - c.S(10.0f);
        for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
            ImGui::PushID(i);
            if (view.focusPending && interactive && i == view.selected) {
                ImGui::SetKeyboardFocusHere();
                view.focusPending = false;
            }
            const bool selected = (i == view.selected);
            if (ImGui::Selectable("##row", selected, interactive ? ImGuiSelectableFlags_None : ImGuiSelectableFlags_Disabled, { rowW, c.S(kRowHeight) })) {
                view.selected = i;
                result.followerActivated = true;
            }
            if (interactive && ImGui::IsItemFocused()) {
                if (view.selected != i) result.followerActivated = true;
                view.selected = i;
            }
            DrawRow(c, listDl, *rows[i], ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), selected,
                interactive && ImGui::IsItemFocused() && ImGui::GetIO().NavVisible);
            ImGui::PopID();
            ImGui::Dummy({ 0.0f, c.S(kRowGap) });
        }
        ImGui::EndChild();

        // Footer
        const float footerY = max.y - c.S(64.0f);
        const float buttons = c.S(kFooterButton) * 2.0f + c.S(12.0f) * 2.0f;
        ImGui::SetCursorScreenPos({ min.x, footerY });
        result.newBondClicked = widgets::DashedButton(c, "##newbond", { max.x - min.x - buttons, c.S(64.0f) }, "New bond", "Write an unwritten page", interactive);
        ImGui::SetCursorScreenPos({ max.x - c.S(kFooterButton) * 2.0f - c.S(12.0f), footerY + c.S(4.0f) });
        result.settingsClicked = widgets::IconButton(c, "##settings", widgets::Icon::Gear, kFooterButton, settingsActive, interactive);
        ImGui::SetCursorScreenPos({ max.x - c.S(kFooterButton), footerY + c.S(4.0f) });
        result.refreshClicked = widgets::IconButton(c, "##refresh", widgets::Icon::Refresh, kFooterButton, false, interactive);
        return result;
    }
}
