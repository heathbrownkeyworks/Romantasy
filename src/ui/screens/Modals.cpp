#include "ui/screens/Modals.h"

#include <imgui.h>

#include <algorithm>

namespace romantasy::ui
{
    namespace
    {
        constexpr const char* kPasswordCopy = "These utilities can rewrite every bond. Enter the password to unlock them.";
        constexpr const char* kPasswordError = "The seal does not recognize that password.";

        struct FooterButtons
        {
            bool confirm = false;
            bool cancel = false;
        };

        // Footer band inside the card: rule, dark wash, Cancel + confirm right-aligned.
        FooterButtons DrawFooter(const widgets::Ctx& c, ImDrawList* dl, ImVec2 min, ImVec2 max, float bandTop, const char* confirmLabel, widgets::ButtonKind kind, bool enabled)
        {
            dl->AddRectFilled({ min.x, bandTop }, max, c.A(theme::WithAlpha(theme::Obsidian, 0.2f)), c.S(theme::RadiusXxl), ImDrawFlags_RoundCornersBottom);
            dl->AddLine({ min.x, bandTop }, { max.x, bandTop }, c.A(theme::WithAlpha(theme::Fg1, 0.06f)), 1.0f);
            const float bw = widgets::ButtonWidth(c, confirmLabel, kind), cw = widgets::ButtonWidth(c, "Cancel", widgets::ButtonKind::Ghost);
            const float y = bandTop + c.S(14.0f);
            FooterButtons r;
            ImGui::SetCursorScreenPos({ max.x - c.S(22.0f) - bw - c.S(10.0f) - cw, y });
            r.cancel = widgets::Button(c, "##cancel", "Cancel", widgets::ButtonKind::Ghost, enabled);
            ImGui::SetCursorScreenPos({ max.x - c.S(22.0f) - bw, y });
            r.confirm = widgets::Button(c, "##confirm", confirmLabel, kind, enabled);
            return r;
        }
    }

    ModalCopy BondModalCopy(LedgerModal kind) noexcept
    {
        if (kind == LedgerModal::ConfirmReset) {
            return { "Player-created bond", "Reset this bond?",
                "The personality remains. This companion's points and recent history reset in the current save only.",
                "Reset bond", widgets::ButtonKind::Secondary };
        }
        return { "Player-created bond", "Remove this bond?",
            "This removes the bond from all saves using this configuration. Personalities supplied by follower mods remain protected.",
            "Remove bond", widgets::ButtonKind::Danger };
    }

    bool CheckDeveloperPassword(std::string_view text) noexcept
    {
        return text == "darthmaul666";
    }

    ModalResult DrawModal(const widgets::Ctx& base, LedgerViewState& view, const theme::Canvas& canvas, float dt, bool interactive)
    {
        ModalResult result;
        if (view.modal == LedgerModal::None) return result;
        const float inP = anim::EaseOut(view.modalIn.Advance(dt));
        float outP = 0.0f;
        if (view.modalClosing) {
            outP = anim::EaseSoft(view.modalOut.Advance(dt));
            if (!view.modalOut.Running()) {
                view.modal = LedgerModal::None;
                view.modalClosing = false;
                view.focusPending = true;  // hand nav focus back to the roster
                return result;
            }
        }
        const float alpha = std::clamp(inP * (1.0f - outP), 0.0f, 1.0f);
        const float scale = (0.94f + 0.06f * inP) * (1.0f - 0.03f * outP);
        // A scaled canvas scales every c.S() uniformly, so the card and its
        // contents grow together about the display centre.
        theme::Canvas scaled = base.canvas;
        scaled.scale *= scale;
        const widgets::Ctx c{ base.fonts, scaled, base.alpha * alpha };
        const bool live = interactive && !view.modalClosing;
        const bool isPassword = view.modal == LedgerModal::Password;

        ImGui::SetNextWindowPos({ 0.0f, 0.0f });
        ImGui::SetNextWindowSize(canvas.display);
        if (view.modalFocusPending) ImGui::SetNextWindowFocus();
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::Begin("##romantasy-modal", nullptr, flags);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled({ 0.0f, 0.0f }, canvas.display, base.A(theme::WithAlpha(theme::Obsidian, 0.55f * alpha)));

        const float pad = c.S(26.0f);
        const float cardW = c.S(isPassword ? 400.0f : 448.0f);
        const float innerW = cardW - pad * 2.0f;
        const float bandH = c.S(14.0f) * 2.0f + c.S(38.0f);
        float cardH = 0.0f;
        float copyH = 0.0f;
        ModalCopy copy{};
        if (isPassword) {
            copyH = widgets::WrappedHeight(c.fonts.body, c.S(12.5f), kPasswordCopy, innerW);
            cardH = pad + c.S(46.0f + 14.0f + 18.0f + 32.0f) + copyH + c.S(16.0f + 44.0f + 10.0f + 16.0f + 16.0f) + bandH;
        } else {
            copy = BondModalCopy(view.modal);
            copyH = widgets::WrappedHeight(c.fonts.body, c.S(12.0f), copy.body, innerW);
            cardH = pad + c.S(22.0f + 30.0f) + copyH + c.S(20.0f) + bandH;
        }
        const ImVec2 center(canvas.display.x * 0.5f, canvas.display.y * 0.5f);
        const ImVec2 min(center.x - cardW * 0.5f, center.y - cardH * 0.5f), max(center.x + cardW * 0.5f, center.y + cardH * 0.5f);
        theme::DrawGlow(dl, min, max, c.S(theme::RadiusXxl), c.A(theme::WithAlpha(theme::Amber, 0.35f)), c.S(26.0f));
        theme::DrawPanel(dl, min, max, c.S(theme::RadiusXxl), c.A(theme::ObsidianLight), c.A(theme::WithAlpha(theme::Fg1, 0.10f)), 1.0f);

        float y = min.y + pad;
        if (!live) ImGui::BeginDisabled();
        if (isPassword) {
            const ImVec2 tileMin(min.x + pad, y), tileMax(tileMin.x + c.S(46.0f), tileMin.y + c.S(46.0f));
            theme::DrawGlow(dl, tileMin, tileMax, c.S(12.0f), c.A(theme::WithAlpha(theme::Amber, 0.18f)), c.S(8.0f));
            theme::DrawPanel(dl, tileMin, tileMax, c.S(12.0f), c.A(theme::SurfaceLight), c.A(theme::WithAlpha(theme::Amber, 0.3f)), 1.0f);
            widgets::DrawIcon(dl, widgets::Icon::Lock, { (tileMin.x + tileMax.x) * 0.5f, (tileMin.y + tileMax.y) * 0.5f }, c.S(22.0f), c.A(theme::Amber), std::max(1.0f, c.S(1.8f)));
            y = tileMax.y + c.S(14.0f);
            widgets::Eyebrow(c, dl, { min.x + pad, y }, "Restricted", theme::Amber);
            y += c.S(18.0f);
            widgets::TrackedText(dl, c.fonts.ceremonial, c.S(21.0f), { min.x + pad, y }, c.A(theme::AmberLight), "DEVELOPER TOOLS", c.S(21.0f) * 0.04f);
            y += c.S(32.0f);
            widgets::WrappedText(c, dl, c.fonts.body, c.S(12.5f), { min.x + pad, y }, c.A(theme::Fg2), kPasswordCopy, innerW);
            y += copyH + c.S(16.0f);
            ImGui::SetCursorScreenPos({ min.x + pad, y });
            if (view.modalFocusPending) ImGui::SetKeyboardFocusHere();
            const bool submitted = widgets::TextField(c, "##password", view.password, sizeof(view.password), { innerW, c.S(44.0f) }, "Password", true);
            if (ImGui::IsItemEdited()) view.passwordRejected = false;
            y += c.S(44.0f) + c.S(10.0f);
            if (view.passwordRejected) theme::DrawText(dl, c.fonts.body, c.S(11.0f), { min.x + pad, y }, c.A(theme::RoseSoft), kPasswordError);
            y += c.S(16.0f) + c.S(16.0f);
            const FooterButtons footer = DrawFooter(c, dl, min, max, y, "Unlock", widgets::ButtonKind::Primary, live);
            if (live && (submitted || footer.confirm)) {
                if (CheckDeveloperPassword(view.password)) {
                    result.confirmed = true;
                    view.CloseModal();
                } else {
                    view.passwordRejected = true;
                }
            }
            if (live && footer.cancel) {
                result.cancelled = true;
                view.CloseModal();
            }
        } else {
            widgets::Eyebrow(c, dl, { min.x + pad, y }, copy.eyebrow, theme::Amber);
            y += c.S(22.0f);
            widgets::TrackedText(dl, c.fonts.ceremonial, c.S(19.0f), { min.x + pad, y }, c.A(theme::Fg1), widgets::Uppercase(copy.title), c.S(19.0f) * 0.06f);
            y += c.S(30.0f);
            widgets::WrappedText(c, dl, c.fonts.body, c.S(12.0f), { min.x + pad, y }, c.A(theme::Fg2), copy.body, innerW);
            y += copyH + c.S(20.0f);
            const FooterButtons footer = DrawFooter(c, dl, min, max, y, copy.confirm, copy.confirmKind, live);
            if (live && footer.confirm) {
                result.confirmed = true;
                view.CloseModal();
            }
            if (live && footer.cancel) {
                result.cancelled = true;
                view.CloseModal();
            }
        }
        if (!live) ImGui::EndDisabled();
        view.modalFocusPending = false;
        ImGui::End();
        return result;
    }
}
