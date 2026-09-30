#pragma once

#include "ui/screens/Ledger.h"

#include <optional>

namespace romantasy::ui
{
    struct DebugButton
    {
        const char* label;
        widgets::ButtonKind kind;
        std::int32_t points;  // DebugAddPoints delta, 0 when unused
        const char* stat;     // DebugApplyStat name, "" when unused
        std::int32_t delta;
        bool refresh;         // Refresh -> LedgerActions::refreshRequested
    };

    inline constexpr int DebugButtonCount = 5;
    [[nodiscard]] const DebugButton& DebugButtonAt(int index) noexcept;
    // The request a button emits; empty for Refresh and out-of-range indices.
    [[nodiscard]] std::optional<DebugRequest> DebugRequestFor(int index);

    // The unlocked panel under the settings card, spanning the pane width from `top`.
    // Fills actions.debug / actions.refreshRequested.
    void DrawDevToolsPanel(const widgets::Ctx& c, const ShellLayout& layout, float top, bool interactive, LedgerActions& actions);
}
