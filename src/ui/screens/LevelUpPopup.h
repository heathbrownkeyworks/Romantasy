#pragma once

#include "ui/screens/Anim.h"
#include "ui/screens/Theme.h"
#include "ui/screens/UiState.h"

namespace romantasy::ui
{
    inline constexpr float PopupAutoDismissSeconds = 20.0f;

    struct PopupViewState
    {
        anim::Tween in;
        anim::Tween out;
        float lifetime = 0.0f;
        float time = 0.0f;
        bool dismissing = false;

        void Reset() noexcept
        {
            in.Start(0.20f);
            out = {};
            lifetime = 0.0f;
            time = 0.0f;
            dismissing = false;
        }

        void Dismiss() noexcept
        {
            if (dismissing) {
                return;
            }
            dismissing = true;
            out.Start(0.16f);
        }
    };

    struct PopupResult
    {
        bool finished = false;
    };

    // Draws the level-change popup for one frame. Dismisses on Enter, Space,
    // Escape, gamepad A or B (when interactive) or after PopupAutoDismissSeconds.
    // dimBackground = true draws a scrim over the canvas (used inside the ledger).
    PopupResult DrawLevelUpPopup(
        const LevelChange& change,
        PopupViewState& view,
        const theme::Fonts& fonts,
        const theme::Canvas& canvas,
        float dt,
        bool interactive,
        bool dimBackground,
        bool controllerActive = false);
}
