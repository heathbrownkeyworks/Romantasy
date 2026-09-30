#pragma once

#include "RE/B/BSSoundHandle.h"

#include "ui/screens/Ledger.h"
#include "ui/screens/LevelUpPopup.h"
#include "ui/screens/UiState.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>

// Facade between RomanceManager and the ImGui menus. Same public surface the
// rest of the plugin used before this rewrite; internals now drive two RE::IMenus.
class RomantasyUI
{
public:
    static RomantasyUI& GetSingleton();

    void Initialize();                          // kDataLoaded: registers both menus
    void Toggle();                              // Ctrl+R or Favorites power
    [[nodiscard]] bool IsOpen() const;          // ledger menu currently shown (false while a show is still pending)

    void SendState(std::string_view message);   // message-only update
    void SendState(const nlohmann::json& state);// full BuildStateJson payload (any thread)
    void ShowLevelUpModal(const nlohmann::json& payload);  // main thread
    void HandleLevelUpDismissed();

    // Called by LedgerMenu / PopupMenu on the UI thread.
    void DrawLedgerFrame();
    void DrawPopupFrame();
    void OnLedgerShown();
    void OnLedgerHidden();
    void OnPopupShown();
    void OnPopupHidden();
    void RequestClose();
    void RequestPopupDismiss();

private:
    RomantasyUI() = default;

    void ShowLevelUpModalNow(const romantasy::ui::LevelChange& change);
    void ShowMenu(std::string_view menuName, bool show);
    void DropUndrawnPopup();
    void PullPendingState();
    void ApplySettingsChange(const romantasy::ui::SettingsChange& change);  // UI thread
    // Bond and debug requests mutate the romance layer, so they are queued to
    // the main thread through the SKSE task interface; the outcome message
    // returns through the state mailbox.
    void ApplyBondRequest(const romantasy::ui::BondRequest& request);   // UI thread
    void ApplyDebugRequest(const romantasy::ui::DebugRequest& request); // UI thread
    void ApplyDeveloperTools(bool unlocked);                             // UI thread
    void PlayModalMusic(bool isLoss);
    void StopModalMusic(std::uint16_t fadeTimeMs = 500);

    mutable std::mutex _stateLock;
    std::optional<romantasy::ui::UiState> _pendingState;
    std::optional<std::string> _pendingMessage;
    romantasy::ui::UiState _liveState;
    romantasy::ui::LedgerViewState _ledgerView;

    std::optional<romantasy::ui::LevelChange> _activePopup;
    romantasy::ui::PopupViewState _popupView;
    std::deque<romantasy::ui::LevelChange> _levelUpQueue;

    RE::BSSoundHandle _modalMusic;
    std::atomic<bool> _isOpen{ false };
    std::atomic<bool> _ledgerPending{ false };
    std::atomic<bool> _popupMenuOpen{ false };
    std::atomic<bool> _closePending{ false };
    bool _registered = false;
    bool _modalMusicActive = false;

    // UI thread only: stamped onto _liveState in PullPendingState.
    bool _developerToolsUnlocked = false;
    std::uint32_t _revision = 0;

    // UI thread only: mirrors whether we hold the AllowTextInput refcount, so
    // OnLedgerShown()/OnLedgerHidden() can never skew RE::ControlMap's count
    // with an unmatched increment/decrement.
    bool _textInputAllowed = false;

    // Guards against a show/hide request that never lands (F3). Atomic because
    // Toggle()/RequestClose() may run on the input thread.
    std::atomic<std::int64_t> _pendingSinceMs{ 0 };

    // Main/UI thread only (ShowLevelUpModal, ShowLevelUpModalNow, both draw
    // frames, DropUndrawnPopup), so plain members are safe here.
    bool _popupDrawn = false;
    std::chrono::steady_clock::time_point _popupRequestedAt{};
};
