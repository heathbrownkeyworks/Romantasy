#pragma once

#include "romance/Pacing.h"
#include "ui/Whispers.h"

#include <atomic>
#include <string_view>

// Global Romantasy settings persisted to Data/SKSE/Plugins/Romantasy/settings.json.
// Persists input mode (Ctrl+R hotkey vs Favorites-menu power) and dashboard toggles.
class Settings
{
public:
    static Settings& GetSingleton();

    void Load();
    void Save() const;
    // Applies the log level: debug when "Detailed log" is on, info otherwise (the default logger's level and flush level).
    void ApplyLogLevel() const;

    [[nodiscard]] bool OpenWithFavorites() const { return _openWithFavorites; }
    void SetOpenWithFavorites(bool value) { _openWithFavorites = value; }

    [[nodiscard]] bool ShowGainModals() const { return _showGainModals; }
    void SetShowGainModals(bool value) { _showGainModals = value; }

    [[nodiscard]] bool ShowLossModals() const { return _showLossModals; }
    void SetShowLossModals(bool value) { _showLossModals = value; }

    [[nodiscard]] bool ShowAwayFollowers() const { return _showAwayFollowers; }
    void SetShowAwayFollowers(bool value) { _showAwayFollowers = value; }

    [[nodiscard]] bool DetailedLog() const { return _detailedLog.load(); }
    void SetDetailedLog(bool value) { _detailedLog = value; }

    [[nodiscard]] romantasy::pacing::Pace Pace() const { return _pace.load(); }
    void SetPace(romantasy::pacing::Pace value) { _pace = value; }

    [[nodiscard]] romantasy::pacing::TierMoment TierMoment() const { return _tierMoment.load(); }
    void SetTierMoment(romantasy::pacing::TierMoment value) { _tierMoment = value; }

    [[nodiscard]] bool Whispers() const { return _whispers.load(); }
    void SetWhispers(bool value) { _whispers = value; }

    [[nodiscard]] romantasy::ui::whispers::WhisperCorner WhisperCorner() const { return _whisperCorner.load(); }
    void SetWhisperCorner(romantasy::ui::whispers::WhisperCorner value) { _whisperCorner = value; }

    // The roster order and the ledger size are kept as their canonical key text. A setter stores only the view the
    // parser returns (a string literal), never its argument; an unknown value is ignored.
    [[nodiscard]] std::string_view RosterOrder() const { return _rosterOrder.load(); }
    void SetRosterOrder(std::string_view value);

    [[nodiscard]] std::string_view LedgerSize() const { return _ledgerSize.load(); }
    void SetLedgerSize(std::string_view value);

    [[nodiscard]] bool HighContrast() const { return _highContrast.load(); }
    void SetHighContrast(bool value) { _highContrast = value; }

    [[nodiscard]] bool ReduceMotion() const { return _reduceMotion.load(); }
    void SetReduceMotion(bool value) { _reduceMotion = value; }

    // The SkyrimNet bridge: read by the bridge on the game thread and by the page's state; both on by default.
    [[nodiscard]] bool SkyrimNetBridge() const { return _skyrimNetBridge.load(); }
    void SetSkyrimNetBridge(bool value) { _skyrimNetBridge = value; }

    [[nodiscard]] bool SkyrimNetConversation() const { return _skyrimNetConversation.load(); }
    void SetSkyrimNetConversation(bool value) { _skyrimNetConversation = value; }

    // The OStim bridge: shared moments count toward a bond; tier changes and whispers wait out a scene. Both on by default.
    [[nodiscard]] bool OStimMoments() const { return _ostimMoments.load(); }
    void SetOStimMoments(bool value) { _ostimMoments = value; }

    [[nodiscard]] bool OStimHold() const { return _ostimHold.load(); }
    void SetOStimHold(bool value) { _ostimHold = value; }

    // Hand-set in settings.json ("authorMode": true); no row in the ledger. Turns on the Papyrus debug functions.
    [[nodiscard]] bool AuthorMode() const { return _authorMode.load(); }

    // True when settings.json exists but could not be read (defaults are in use).
    [[nodiscard]] bool LoadFailed() const { return _loadFailed; }
    // True when the last Save did not complete; the file on disk is the previous one.
    [[nodiscard]] bool SaveFailed() const { return _saveFailed; }

private:
    Settings() = default;

    bool _openWithFavorites = false;  // default: Ctrl+R hotkey
    bool _showGainModals = true;
    bool _showLossModals = true;
    bool _showAwayFollowers = true;
    // Read on the Papyrus thread and written on the game thread.
    std::atomic_bool _detailedLog = false;
    std::atomic<romantasy::pacing::Pace> _pace = romantasy::pacing::Pace::Standard;
    std::atomic<romantasy::pacing::TierMoment> _tierMoment = romantasy::pacing::TierMoment::Calm;
    std::atomic_bool _authorMode = false;
    std::atomic_bool _whispers = true;
    std::atomic<romantasy::ui::whispers::WhisperCorner> _whisperCorner = romantasy::ui::whispers::WhisperCorner::TopRight;
    std::atomic<std::string_view> _rosterOrder = std::string_view("closest");
    std::atomic<std::string_view> _ledgerSize = std::string_view("standard");
    std::atomic_bool _highContrast = false;
    std::atomic_bool _reduceMotion = false;
    std::atomic_bool _skyrimNetBridge = true;
    std::atomic_bool _skyrimNetConversation = true;
    std::atomic_bool _ostimMoments = true;
    std::atomic_bool _ostimHold = true;
    bool _loadFailed = false;
    mutable bool _saveFailed = false;
};
