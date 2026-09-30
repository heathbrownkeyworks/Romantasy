#pragma once

#include "ui/screens/Ledger.h"

#include <string_view>

namespace romantasy::ui
{
    inline constexpr float NoticeHoldSeconds = 3.4f;

    // "could not" / "cannot" / "protected" anywhere in the message (case-insensitive).
    [[nodiscard]] bool IsWarningMessage(std::string_view message);
    // Once per frame, before drawing: starts the toast when an armed notice sees
    // a new revision with a message, then advances the In / Hold / Out phases.
    void UpdateNotice(NoticeState& notice, const UiState& state, float dt);
    // 0 hidden .. 1 fully shown.
    [[nodiscard]] float NoticeProgress(const NoticeState& notice) noexcept;
    // Bottom-centre toast on the foreground draw list; draws nothing when hidden.
    void DrawNotice(const widgets::Ctx& c, const NoticeState& notice, const theme::Canvas& canvas);
}
