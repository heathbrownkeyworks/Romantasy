#pragma once

#include <array>

// The named channels between the plugin and the page. Mirrors TOPICS, ACTIONS
// and PAYLOAD_KEYS in view/js/bridge.js; tests/view/Contract.mjs keeps the two
// in step.
namespace romantasy::ui::channels
{
    // Plugin -> page.
    inline constexpr const char* kState = "romantasyState";
    inline constexpr const char* kMode = "romantasyMode";
    inline constexpr const char* kLevelChange = "romantasyLevelChange";
    inline constexpr const char* kJournal = "romantasyJournal";

    // Page -> plugin.
    inline constexpr std::array kPageActions{
        "romantasyReady", "romantasyOpen", "romantasyClose", "romantasyRefresh", "romantasySettings", "romantasyBond",
        "romantasyDevUnlock", "romantasyDevLock", "romantasyDebug", "romantasyPopupShown", "romantasyPopupDismissed",
        "romantasyJournalOpen"
    };

    // The keys each action's payload may carry.
    inline constexpr std::array kSettingsKeys{ "openWithFavorites", "showGainModals", "showLossModals", "showAwayFollowers", "detailedLog", "pace", "tierMoment", "whispers", "whisperCorner", "rosterOrder", "ledgerSize", "highContrast", "reduceMotion", "skyrimNetBridge", "skyrimNetConversation", "ostimMoments", "ostimHold" };
    inline constexpr std::array kBondKeys{ "kind", "identity", "preferences" };
    inline constexpr std::array kDevUnlockKeys{ "password" };
    inline constexpr std::array kDebugKeys{ "points", "stat", "delta" };
    inline constexpr std::array kPopupAckKeys{ "seq" };  // romantasyPopupShown and romantasyPopupDismissed
    inline constexpr std::array kJournalKeys{ "identity" };
}
