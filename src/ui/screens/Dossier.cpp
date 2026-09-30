#include "ui/screens/Dossier.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>

namespace romantasy::ui
{
    namespace
    {
        constexpr float kHeroH = 248.0f, kCardsH = 300.0f, kMilestonesH = 200.0f, kGap = 12.0f;

        bool IsPlayerCreated(const FollowerRow& row)
        {
            return row.profileOrigin == "player" && row.personalityEditable;
        }

        void SectionEyebrow(const widgets::Ctx& c, ImDrawList* dl, ImVec2 pos, const char* text, ImU32 color)
        {
            dl->AddLine({ pos.x, pos.y + c.S(6.0f) }, { pos.x + c.S(14.0f), pos.y + c.S(6.0f) }, c.A(theme::WithAlpha(color, 0.6f)), 1.0f);
            widgets::Eyebrow(c, dl, { pos.x + c.S(22.0f), pos.y }, text, color, 11.0f);
        }

        void DrawHero(const widgets::Ctx& c, ImDrawList* dl, const FollowerRow& row, ImVec2 min, float w, float time)
        {
            const ImVec2 max(min.x + w, min.y + c.S(kHeroH));
            widgets::CardFrame(c, dl, min, max);
            widgets::HeroHeart(c, dl, { min.x + c.S(150.0f), min.y + c.S(130.0f) }, c.S(160.0f), time);

            const float tx = min.x + c.S(300.0f);
            const float right = max.x - c.S(32.0f);
            widgets::Eyebrow(c, dl, { tx, min.y + c.S(40.0f) }, row.role.empty() ? "Companion" : row.role, theme::Fg3);
            const std::string name = widgets::ClipText(c.fonts.ceremonial, c.S(44.0f), widgets::Uppercase(row.name), right - tx);
            theme::DrawText(dl, c.fonts.ceremonial, c.S(44.0f), { tx, min.y + c.S(56.0f) }, c.A(theme::Amber), name.c_str());
            const std::string tier = row.levelName.empty() ? tiers::Names[tiers::TierIndex(row.points)] : row.levelName;
            widgets::ScriptTitle(c, dl, { tx, min.y + c.S(104.0f) }, tier.c_str(), 44.0f, theme::Amber);

            const char* pillText = row.isFollowing ? "Following" : "Away";
            const float pillW = widgets::PillWidth(c, pillText, true);
            widgets::Pill(c, dl, { tx, min.y + c.S(176.0f) }, pillText, row.isFollowing ? theme::Emerald : theme::Fg4, true);
            theme::DrawText(dl, c.fonts.bodyBold, c.S(22.0f), { tx + pillW + c.S(14.0f), min.y + c.S(168.0f) }, c.A(theme::Fg1), BondText(row.points).c_str());

            widgets::TierMeter(c, dl, { tx, min.y + c.S(212.0f) }, right - tx, c.S(6.0f), row.points, false);
            theme::DrawText(dl, c.fonts.body, c.S(12.0f), { tx, min.y + c.S(228.0f) }, c.A(theme::Fg3), tier.c_str());
            theme::DrawTextAligned(dl, c.fonts.body, c.S(12.0f), { right, min.y + c.S(228.0f) }, c.A(theme::Fg3), LegendRight(row.points).c_str(), 1.0f);
        }

        void DrawValueCard(const widgets::Ctx& c, ImDrawList* dl, const FollowerRow& row, ImVec2 min, float w, bool interactive)
        {
            const ImVec2 max(min.x + w, min.y + c.S(kCardsH));
            widgets::CardFrame(c, dl, min, max);
            const float x = min.x + c.S(24.0f), right = max.x - c.S(24.0f);
            SectionEyebrow(c, dl, { x, min.y + c.S(22.0f) }, "What they value", theme::Amber);

            const SealInfo seal = SealFor(row);
            const ImVec2 boxMin(x, min.y + c.S(48.0f)), boxMax(right, min.y + c.S(112.0f));
            theme::DrawPanel(dl, boxMin, boxMax, c.S(theme::RadiusMd), c.A(theme::WithAlpha(theme::Surface, 0.6f)), c.A(theme::WithAlpha(seal.color, 0.3f)), 1.0f);
            widgets::SealBadge(c, dl, { boxMin.x + c.S(32.0f), (boxMin.y + boxMax.y) * 0.5f }, c.S(40.0f), seal.label, seal.color);
            theme::DrawText(dl, c.fonts.bodyBold, c.S(13.0f), { boxMin.x + c.S(64.0f), boxMin.y + c.S(10.0f) }, c.A(theme::Fg1), seal.title);
            dl->AddText(c.fonts.body ? c.fonts.body : ImGui::GetFont(), c.S(11.0f), { boxMin.x + c.S(64.0f), boxMin.y + c.S(30.0f) }, c.A(theme::Fg3),
                seal.description.c_str(), nullptr, right - c.S(12.0f) - (boxMin.x + c.S(64.0f)));

            const float colW = (right - x) * 0.5f;
            const float listTop = min.y + c.S(152.0f);
            SectionEyebrow(c, dl, { x, min.y + c.S(130.0f) }, "Deepens the bond", theme::Amber);
            SectionEyebrow(c, dl, { x + colW, min.y + c.S(130.0f) }, "Wounds the bond", theme::Rose);
            ImGui::PushID(RowIdentity(row).c_str());
            ImGui::PushStyleColor(ImGuiCol_NavCursor, c.A(theme::Amber));
            auto list = [&](const char* id, const std::vector<std::string>& items, float colX, widgets::Icon icon, ImU32 iconColor) {
                ImGui::SetCursorScreenPos({ colX, listTop });
                // Keep each list focusable so keyboard and controller navigation
                // can enter it and scroll, including profiles with no action buttons.
                ImGui::BeginChild(id, { colW - c.S(12.0f), max.y - c.S(8.0f) - listTop }, ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoBackground | (interactive ? 0 : ImGuiWindowFlags_NoInputs));
                ImDrawList* listDl = ImGui::GetWindowDrawList();
                const ImVec2 start = ImGui::GetCursorScreenPos();
                const float contentW = ImGui::GetContentRegionAvail().x;
                if (items.empty()) {
                    theme::DrawText(listDl, c.fonts.body, c.S(12.0f), start, c.A(theme::Fg4), "None recorded");
                }
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const float y = start.y + c.S(30.0f) * static_cast<float>(i);
                    widgets::DrawIcon(listDl, icon, { start.x + c.S(7.0f), y + c.S(8.0f) }, c.S(12.0f), c.A(iconColor), std::max(1.0f, c.S(1.5f)));
                    theme::DrawText(listDl, c.fonts.body, c.S(13.0f), { start.x + c.S(24.0f), y }, c.A(theme::Fg1),
                        widgets::ClipText(c.fonts.body, c.S(13.0f), items[i], contentW - c.S(24.0f)).c_str());
                }
                ImGui::Dummy({ contentW, c.S(18.0f + 30.0f * static_cast<float>(items.empty() ? 0 : items.size() - 1)) });
                ImGui::EndChild();
            };
            list("##likes", row.likes, x, widgets::Icon::Heart, theme::Amber);
            list("##dislikes", row.dislikes, x + colW, widgets::Icon::Claw, theme::Rose);
            ImGui::PopStyleColor();
            ImGui::PopID();
        }

        void DrawChangesCard(const widgets::Ctx& c, ImDrawList* dl, const FollowerRow& row, ImVec2 min, float w)
        {
            const ImVec2 max(min.x + w, min.y + c.S(kCardsH));
            widgets::CardFrame(c, dl, min, max);
            const float x = min.x + c.S(24.0f), right = max.x - c.S(24.0f);
            SectionEyebrow(c, dl, { x, min.y + c.S(22.0f) }, "Recent changes", theme::Amber);
            if (row.recent.empty()) {
                theme::DrawText(dl, c.fonts.body, c.S(12.0f), { x, min.y + c.S(60.0f) }, c.A(theme::Fg4), "No deeds recorded yet.");
                return;
            }
            const int count = std::min<int>(static_cast<int>(row.recent.size()), 5);
            for (int i = 0; i < count; ++i) {
                const auto& event = row.recent[i];
                const float y = min.y + c.S(56.0f) + c.S(36.0f) * static_cast<float>(i);
                const std::string delta = (event.points >= 0 ? "+" : "") + FormatPoints(event.points);
                theme::DrawText(dl, c.fonts.bodyBold, c.S(13.0f), { x, y }, c.A(event.points >= 0 ? theme::Amber : theme::Rose), delta.c_str());
                const ImVec2 whenExtent = theme::MeasureText(c.fonts.body, c.S(11.0f), event.when.c_str());
                theme::DrawText(dl, c.fonts.body, c.S(13.0f), { x + c.S(36.0f), y }, c.A(theme::Fg1),
                    widgets::ClipText(c.fonts.body, c.S(13.0f), event.label, right - whenExtent.x - c.S(16.0f) - (x + c.S(36.0f))).c_str());
                theme::DrawTextAligned(dl, c.fonts.body, c.S(11.0f), { right, y + c.S(2.0f) }, c.A(theme::Fg3), event.when.c_str(), 1.0f);
            }
        }

        void DrawMilestones(const widgets::Ctx& c, ImDrawList* dl, const FollowerRow& row, ImVec2 min, float w)
        {
            const ImVec2 max(min.x + w, min.y + c.S(kMilestonesH));
            widgets::CardFrame(c, dl, min, max);
            const float x = min.x + c.S(24.0f);
            SectionEyebrow(c, dl, { x, min.y + c.S(22.0f) }, "Milestones", theme::Amber);
            const int current = tiers::TierIndex(row.points);
            const float gap = c.S(16.0f);
            const float cardW = (w - c.S(48.0f) - gap * 5.0f) / 6.0f;
            const float top = min.y + c.S(48.0f), bottom = min.y + c.S(184.0f);
            for (int i = 0; i < tiers::Count; ++i) {
                const float cx0 = x + (cardW + gap) * static_cast<float>(i);
                const ImVec2 cmin(cx0, top), cmax(cx0 + cardW, bottom);
                const bool reached = row.points >= tiers::Thresholds[i];
                const bool isCurrent = i == current;
                if (isCurrent) theme::DrawGlow(dl, cmin, cmax, c.S(theme::RadiusLg), c.A(theme::WithAlpha(theme::Amber, 0.25f)), c.S(12.0f));
                theme::DrawPanel(dl, cmin, cmax, c.S(theme::RadiusLg),
                    c.A(isCurrent ? theme::WithAlpha(theme::Amber, 0.08f) : theme::WithAlpha(theme::ObsidianLighter, 0.6f)),
                    c.A(isCurrent ? theme::WithAlpha(theme::Amber, 0.8f) : theme::WithAlpha(theme::Fg1, 0.06f)), 1.0f);
                const float cx = (cmin.x + cmax.x) * 0.5f;
                const ImU32 tone = isCurrent ? theme::AmberLight : (reached ? theme::Amber : theme::Fg4);
                if (reached) {
                    theme::DrawHeart(dl, { cx, top + c.S(34.0f) }, c.S(22.0f), c.A(tone), false);
                } else {
                    // A hairline outline alone dissolves at 720p, so back it with a faint fill
                    // and only add the stroke once the heart is wide enough to carry one.
                    theme::DrawHeart(dl, { cx, top + c.S(34.0f) }, c.S(22.0f), c.A(theme::WithAlpha(theme::Fg4, 0.25f)), false);
                    if (c.S(22.0f) >= 20.0f) {
                        theme::DrawHeartOutline(dl, { cx, top + c.S(34.0f) }, c.S(22.0f), c.A(tone), std::max(1.0f, c.S(1.2f)));
                    }
                }
                theme::DrawTextAligned(dl, c.fonts.script, c.S(24.0f), { cx, top + c.S(58.0f) }, c.A(tone), tiers::Names[i], 0.5f);
                theme::DrawTextAligned(dl, c.fonts.body, c.S(11.0f), { cx, top + c.S(92.0f) }, c.A(reached ? theme::Fg3 : theme::MulAlpha(theme::Fg4, 0.7f)), FormatPoints(tiers::Thresholds[i]).c_str(), 0.5f);
                if (isCurrent) {
                    const float ew = widgets::EyebrowWidth(c, "You are here", 8.0f);
                    widgets::Eyebrow(c, dl, { cx - ew * 0.5f, top + c.S(114.0f) }, "You are here", theme::Amber, 8.0f);
                }
            }
        }
    }

    SealInfo SealFor(const FollowerRow& row)
    {
        SealInfo seal;
        const bool playerCreated = IsPlayerCreated(row);
        const bool authorDefined = row.profileOrigin == "author";
        if (!playerCreated && !row.profileFile.empty()) {
            seal.label = "CFG";
            seal.title = "Configured personality";
            seal.description = "Preferences loaded from " + row.profileFile + ".";
            seal.color = theme::Copper;
        } else if (playerCreated) {
            seal.label = "YOU";
            seal.title = "Player-created personality";
            seal.description = "This ledger page belongs to you. Its likes and dislikes may be rewritten.";
            seal.color = theme::Amber;
        } else if (authorDefined) {
            seal.label = "AU";
            seal.title = "Author-defined personality";
            seal.description = "Sealed by the follower author" + (row.sourcePlugin.empty() ? std::string{} : " in " + row.sourcePlugin) + ". Romantasy will not alter or remove it.";
            seal.color = theme::Copper;
        } else {
            seal.label = "AU";
            seal.title = "Managed personality";
            seal.description = "Supplied by another compatible mod at runtime. Romantasy treats it as read-only.";
            seal.color = theme::Copper;
        }
        return seal;
    }

    std::string LegendRight(std::int32_t points)
    {
        const std::int32_t next = tiers::NextThreshold(points);
        if (next < 0) return "Vow sealed in the ledger";
        return FormatPoints(next - points) + " until " + tiers::Names[tiers::TierIndex(points) + 1];
    }

    std::string BondText(std::int32_t points)
    {
        return FormatPointsWithThreshold(points) + " bond";
    }

    DossierResult DrawDossier(const widgets::Ctx& base, const FollowerRow& row, LedgerViewState& view, const ShellLayout& layout, float dt, bool interactive)
    {
        DossierResult result;
        const float slide = anim::EaseOut(view.dossierSlide.Advance(dt));
        const widgets::Ctx c{ base.fonts, base.canvas, base.alpha * std::max(slide, 0.05f) };
        const float shift = -(1.0f - slide) * c.S(24.0f);

        const ImVec2 paneMin = layout.rightMin, paneMax = layout.rightMax;
        ImGui::SetCursorScreenPos(paneMin);
        // NavFlattened (not NoNav) so the D-pad can reach the action buttons.
        ImGui::BeginChild("##dossier", { paneMax.x - paneMin.x, paneMax.y - paneMin.y }, ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetCursorScreenPos();  // scroll-adjusted
        const float w = paneMax.x - paneMin.x - c.S(10.0f);
        float y = origin.y;
        const float x = origin.x + shift;

        DrawHero(c, dl, row, { x, y }, w, view.time);
        y += c.S(kHeroH + kGap);
        const float half = (w - c.S(24.0f)) * 0.5f;
        DrawValueCard(c, dl, row, { x, y }, half, interactive);
        DrawChangesCard(c, dl, row, { x + half + c.S(24.0f), y }, half);
        y += c.S(kCardsH + kGap);
        DrawMilestones(c, dl, row, { x, y }, w);
        y += c.S(kMilestonesH);

        const bool playerCreated = IsPlayerCreated(row);
        if (playerCreated) {
            y += c.S(10.0f);
            const float gap = c.S(10.0f);
            const float editW = widgets::ButtonWidth(c, "Edit personality", widgets::ButtonKind::Secondary);
            const float resetW = widgets::ButtonWidth(c, "Reset bond", widgets::ButtonKind::Ghost);
            const float removeW = widgets::ButtonWidth(c, "Remove bond", widgets::ButtonKind::Danger);
            float bx = x + w - removeW;
            ImGui::SetCursorScreenPos({ bx, y });
            result.removeRequested = widgets::Button(c, "##remove", "Remove bond", widgets::ButtonKind::Danger, interactive && row.removable);
            bx -= gap + resetW;
            ImGui::SetCursorScreenPos({ bx, y });
            result.resetRequested = widgets::Button(c, "##reset", "Reset bond", widgets::ButtonKind::Ghost, interactive && row.removable);
            bx -= gap + editW;
            ImGui::SetCursorScreenPos({ bx, y });
            result.editRequested = widgets::Button(c, "##edit", "Edit personality", widgets::ButtonKind::Secondary, interactive);
            y += c.S(38.0f) + c.S(10.0f);
        } else {
            y += c.S(8.0f);
        }

        ImGui::SetCursorScreenPos({ origin.x, y });
        ImGui::Dummy({ w, 1.0f });  // establishes the scroll extent
        ImGui::EndChild();
        return result;
    }

    bool DrawEmptyPage(const widgets::Ctx& c, const ShellLayout& layout, bool interactive)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float cx = (layout.rightMin.x + layout.rightMax.x) * 0.5f;
        const float top = layout.rightMin.y + (layout.rightMax.y - layout.rightMin.y) * 0.30f;
        theme::DrawHeartOutline(dl, { cx, top }, c.S(120.0f), c.A(theme::Fg4), std::max(1.0f, c.S(1.5f)));
        theme::DrawTextAligned(dl, c.fonts.ceremonial, c.S(26.0f), { cx, top + c.S(90.0f) }, c.A(theme::Fg1), "No bonds recorded", 0.5f);
        theme::DrawTextAligned(dl, c.fonts.body, c.S(13.0f), { cx, top + c.S(130.0f) }, c.A(theme::Fg3), "Walk with a companion and open a new page.", 0.5f);
        ImGui::SetCursorScreenPos({ cx - c.S(160.0f), top + c.S(170.0f) });
        return widgets::DashedButton(c, "##addcompanion", { c.S(320.0f), c.S(64.0f) }, "Add a companion", "Write an unwritten page", interactive);
    }
}
