#pragma once

#include "ui/screens/Ledger.h"

#include <vector>

namespace romantasy::ui
{
    struct RosterResult
    {
        bool followerActivated = false;
        bool settingsClicked = false;
        bool refreshClicked = false;
        bool newBondClicked = false;
    };

    // Left column: header, scrollable rows, footer. `rows` are the visible followers.
    RosterResult DrawRoster(const widgets::Ctx& c, const UiState& state, const std::vector<const FollowerRow*>& rows,
        LedgerViewState& view, const ShellLayout& layout, bool interactive, bool settingsActive);
}
