#include "ui/screens/Ledger.h"

#include "ui/screens/Roster.h"
#include "ui/screens/Dossier.h"
#include "ui/screens/Enrollment.h"
#include "ui/screens/SettingsPane.h"
#include "ui/screens/Modals.h"
#include "ui/screens/Notice.h"
#include "ui/screens/ControllerHints.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace romantasy::ui
{
    std::string FormatPoints(std::int32_t points)
    {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "%d", points < 0 ? -points : points);
        std::string digits(buffer);
        std::string out;
        const std::size_t n = digits.size();
        for (std::size_t i = 0; i < n; ++i) {
            out.push_back(digits[i]);
            const std::size_t remaining = n - i - 1;
            if (remaining > 0 && remaining % 3 == 0) out.push_back(',');
        }
        return points < 0 ? "-" + out : out;
    }

    std::string FormatPointsWithThreshold(std::int32_t points)
    {
        const std::int32_t next = tiers::NextThreshold(points);
        return next < 0 ? FormatPoints(points) : FormatPoints(points) + " / " + FormatPoints(next);
    }

    PaneResult DrawPaneContent(const widgets::Ctx& c, const UiState& state, const FollowerRow* selected, LedgerViewState& view,
        const ShellLayout& layout, bool interactive, float dt, LedgerActions& actions)
    {
        PaneResult result;
        const float swap = anim::EaseOut(view.paneSwap.Advance(dt));
        const widgets::Ctx pc{ c.fonts, c.canvas, c.alpha * std::max(swap, 0.05f) };
        ShellLayout risen = layout;
        const float rise = (1.0f - swap) * c.S(12.0f);
        risen.rightMin.y += rise;
        risen.rightMax.y += rise;
        if (view.pane == LedgerPane::Settings) {
            DrawSettingsPane(pc, state, view, risen, interactive, dt, actions);
        } else if (selected) {
            const DossierResult dossier = DrawDossier(pc, *selected, view, risen, dt, interactive);
            result.editRequested = dossier.editRequested;
            result.resetRequested = dossier.resetRequested;
            result.removeRequested = dossier.removeRequested;
        } else {
            result.addCompanionRequested = DrawEmptyPage(pc, risen, interactive);
        }
        return result;
    }

    LedgerActions DrawLedger(const UiState& state, LedgerViewState& view, const theme::Fonts& fonts, const theme::Canvas& canvas, float dt, bool interactive, bool controllerActive)
    {
        LedgerActions actions;
        view.time += dt;
        // Last frame's state: while a field is active, Escape belongs to it.
        const bool typing = ImGui::GetIO().WantTextInput;
        UpdateNotice(view.notice, state, dt);
        const float alpha = anim::EaseOut(view.appIn.Advance(dt));
        const widgets::Ctx c{ fonts, canvas, alpha };
        const float bottomInset = controllerActive ? canvas.S(ControllerHintsHeight) : 0.0f;
        const ShellLayout layout = ComputeShellLayout(canvas, bottomInset);
        const bool modalOpen = view.modal != LedgerModal::None;
        const bool takeover = view.screen == LedgerScreen::Enrollment;
        const bool consoleLive = interactive && !modalOpen && !takeover;

        std::vector<const FollowerRow*> rows;
        for (const auto& follower : state.followers) {
            if (state.showAwayFollowers || follower.isFollowing) rows.push_back(&follower);
        }
        view.selected = rows.empty() ? 0 : std::clamp(view.selected, 0, static_cast<int>(rows.size()) - 1);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, canvas.S(8.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, 0);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, c.A(theme::WithAlpha(theme::Fg1, 0.04f)));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, c.A(theme::WithAlpha(theme::Fg1, 0.08f)));
        ImGui::PushStyleColor(ImGuiCol_NavCursor, 0);  // widgets draw their own amber rings
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, 0);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, c.A(theme::Copper));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, c.A(theme::CopperLight));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, c.A(theme::AmberLight));

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::SetNextWindowPos(layout.min);
        ImGui::SetNextWindowSize({ layout.max.x - layout.min.x, layout.max.y - layout.min.y });
        ImGui::Begin("##romantasy-ledger", nullptr, flags);

        DrawShellBackdrop(c, ImGui::GetWindowDrawList(), layout);

        const RosterResult roster = DrawRoster(c, state, rows, view, layout, consoleLive, view.pane == LedgerPane::Settings);
        const FollowerRow* selectedRow = rows.empty() ? nullptr : rows[view.selected];
        if (roster.followerActivated) view.pane = LedgerPane::Dossier;
        if (roster.settingsClicked) view.pane = (view.pane == LedgerPane::Settings) ? LedgerPane::Dossier : LedgerPane::Settings;
        if (roster.refreshClicked) actions.refreshRequested = true;

        if (view.selected != view.lastSelected) {
            view.dossierSlide.Start(0.45f);
            view.lastSelected = view.selected;
        }
        if (view.pane != view.lastPane) {
            view.paneSwap.Start(0.20f);
            view.lastPane = view.pane;
        }

        const PaneResult pane = DrawPaneContent(c, state, selectedRow, view, layout, consoleLive, dt, actions);
        if (selectedRow && pane.resetRequested) view.OpenModal(LedgerModal::ConfirmReset, RowIdentity(*selectedRow));
        if (selectedRow && pane.removeRequested) view.OpenModal(LedgerModal::ConfirmRemove, RowIdentity(*selectedRow));
        if (actions.refreshRequested) view.notice.Arm(state.revision);
        if (roster.newBondClicked || pane.addCompanionRequested) OpenEnrollment(state, view, EnrollMode::Create, {});
        if (selectedRow && pane.editRequested) OpenEnrollment(state, view, EnrollMode::Edit, RowIdentity(*selectedRow));

        // Re-read rather than the frame-start `takeover`: OpenEnrollment above may have
        // just opened the takeover this same frame, and the stale false would draw a
        // second close glyph over the takeover's own. (The mirror case - the close fade
        // finishing this frame inside DrawEnrollment below, flipping back to Console - is
        // a known, accepted one-frame gap: neither glyph draws that single frame, since
        // this check already ran against the frame-start `takeover == true`.)
        if (view.screen != LedgerScreen::Enrollment && DrawShellClose(c, layout, consoleLive)) actions.closeRequested = true;
        ImGui::End();

        // Takeover: its own window, submitted after the ledger so it renders above the child windows.
        if (view.screen == LedgerScreen::Enrollment) {
            const EnrollmentResult enroll = DrawEnrollment(c, state, view, layout, interactive && !modalOpen, dt);
            if (enroll.committed) {
                actions.bond = MakeEnrollmentRequest(state, view.enroll);
                view.notice.Arm(state.revision);
                CloseEnrollment(view);
            } else if (enroll.cancelled) {
                CloseEnrollment(view);
            }
            if (enroll.closeRequested) actions.closeRequested = true;
        }

        // Modal: also its own window, above everything but the notice and the popup.
        // Captured before DrawModal (which may call CloseModal on confirm) so the
        // conversion below never depends on what CloseModal leaves in view.modal.
        const LedgerModal modalKind = view.modal;
        const std::string modalIdentity = view.modalIdentity;
        const ModalResult modal = DrawModal(c, view, canvas, dt, interactive);
        if (modal.confirmed && (modalKind == LedgerModal::ConfirmReset || modalKind == LedgerModal::ConfirmRemove)) {
            BondRequest request;
            request.kind = modalKind == LedgerModal::ConfirmReset ? BondRequest::Kind::Reset : BondRequest::Kind::Remove;
            request.identity = modalIdentity;
            actions.bond = std::move(request);
            view.notice.Arm(state.revision);
        }
        if (modal.confirmed && modalKind == LedgerModal::Password) {
            actions.developerTools = true;  // CheckDeveloperPassword already passed inside DrawModal
        }

        // Escape / B priority: modal, then the takeover, then the console. (A popup
        // overlay already passed interactive = false.) An open modal closes on the
        // first press even while its own field is active (e.g. the password modal's
        // auto-focused field); only the takeover and the console defer to an active
        // field, letting it own Escape itself. Re-read view.modal here rather than
        // the cached modalOpen: DrawModal may have just flipped it to None this same
        // frame (fade-out completing).
        if (interactive && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))) {
            if (view.modal != LedgerModal::None) view.CloseModal();            // a modal closes even while its field is active
            else if (typing) { /* the field owns Escape: it deactivates itself */ }
            else if (view.screen == LedgerScreen::Enrollment) CloseEnrollment(view);
            else actions.closeRequested = true;
        }

        theme::Canvas noticeCanvas = canvas;
        noticeCanvas.display.y -= bottomInset;
        DrawNotice(c, view.notice, noticeCanvas);
        if (controllerActive && interactive) {
            const auto scope = view.modal != LedgerModal::None ? (typing ? ControllerHintScope::ModalTextField : ControllerHintScope::Modal) :
                (typing ? ControllerHintScope::TextField :
                    (view.screen == LedgerScreen::Enrollment ? ControllerHintScope::Enrollment : ControllerHintScope::Console));
            DrawControllerHints(c, scope);
        }

        ImGui::PopStyleColor(8);
        ImGui::PopStyleVar(5);
        return actions;
    }
}
