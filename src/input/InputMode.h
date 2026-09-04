#pragma once

namespace InputMode
{
    // (Re)register or unregister the Ctrl+R hotkey to match Settings. Touches only
    // the input sink, so it is safe to call before a save is loaded.
    void RefreshHotkey();

    // Full apply once a game is loaded (player exists): matches the hotkey AND
    // grants/removes + (un)favorites the ROM_PowerUI spell to match Settings.
    void ApplyForLoadedGame();
}
