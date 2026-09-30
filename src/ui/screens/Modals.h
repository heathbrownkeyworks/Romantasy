#pragma once

#include "ui/screens/Ledger.h"

#include <string_view>

namespace romantasy::ui
{
    struct ModalCopy
    {
        const char* eyebrow;
        const char* title;
        const char* body;
        const char* confirm;
        widgets::ButtonKind confirmKind;
    };

    // Bond-confirmation text for ConfirmReset / ConfirmRemove.
    [[nodiscard]] ModalCopy BondModalCopy(LedgerModal kind) noexcept;
    // Validate the developer-tools confirmation phrase.
    [[nodiscard]] bool CheckDeveloperPassword(std::string_view text) noexcept;

    struct ModalResult
    {
        bool confirmed = false;  // confirm pressed (or the password was accepted)
        bool cancelled = false;
    };

    // Draws the scrim and the card for view.modal in its own top-level window
    // (None draws nothing). Confirm / Cancel, or Enter and an accepted password
    // in the password modal, start the close; the modal becomes None when the
    // fade out finishes.
    ModalResult DrawModal(const widgets::Ctx& c, LedgerViewState& view, const theme::Canvas& canvas, float dt, bool interactive);
}
