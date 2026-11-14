#pragma once

namespace InputMode
{
    // (Re)register or unregister the Ctrl+R hotkey to match Settings. Touches only
    // the input sink, so it is safe to call before a save is loaded.
    void RefreshHotkey();

    // Full apply once a game is loaded (player exists): matches the hotkey AND
    // grants/removes + (un)favorites the ROM_PowerUI spell to match Settings.
    void ApplyForLoadedGame();

    // ROM_PowerUI: CS_Romantasy.esp's 0x83C, or by editor ID when that fails. Null before kDataLoaded or when missing.
    [[nodiscard]] RE::SpellItem* LookupPower();

    // LookupPower() found the power (once found, the answer is kept), whatever the open mode and whether or not a game
    // is loaded. After kDataLoaded.
    [[nodiscard]] bool PowerFound();
}
