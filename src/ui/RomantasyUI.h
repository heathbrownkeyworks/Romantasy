#pragma once

#include "RE/B/BSSoundHandle.h"

#include "ui/PageRequests.h"
#include "ui/PopupQueue.h"
#include "ui/Whispers.h"

#include <atomic>
#include <cstdint>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

// Facade between RomanceManager and the ledger page that Magelight UI hosts (ui/magelight/MagelightHost). It keeps the
// level-change popup queue, the settings gate on popups, the popup music, the developer-tools flag and the request
// handlers. Its state lives on the game thread; SendState and ShowLevelUpModal may be called from anywhere and post
// themselves there.
class RomantasyUI
{
public:
    static RomantasyUI& GetSingleton();

    void Initialize();                          // kDataLoaded: the Magelight view and the menu sink
    void Toggle();                              // Ctrl+R or the Favorites power (game thread)
    [[nodiscard]] bool IsOpen() const;          // the ledger holds UI mode (any thread)
    void Close();                               // save load or new game: everything closes, the queue is dropped

    void SendState(std::string_view message);   // a state with this message
    void SendState(const nlohmann::json& state);// full BuildStateJson payload (any thread)
    // Any thread. `whisperIfOff`: when the popup for this direction is switched off, or cannot be shown because there is
    // no usable Magelight host, a tier whisper says it instead (false for a developer-tool change). `generation`: WhisperGeneration() when the change was announced.
    void ShowLevelUpModal(const nlohmann::json& payload, bool whisperIfOff, std::uint32_t generation);
    void HandleLevelUpDismissed();              // game thread

    // Whispers (game thread). Whisper pushes one on the queue and pumps. PumpWhispers shows the next one when it may
    // show (on the HUD card, or as a game notification when there is no card), ends one the page has not answered
    // within the queue's time, and hides the card when nothing is on it.
    // `generation`: WhisperGeneration() when it was raised; one raised before the last load is dropped.
    void Whisper(romantasy::ui::whispers::Whisper whisper, std::uint32_t generation);
    void PumpWhispers();
    // Moves on at every new game or load (Close). Any thread.
    [[nodiscard]] std::uint32_t WhisperGeneration() const { return _whisperGeneration.load(); }

private:
    RomantasyUI() = default;

    // MagelightHost callbacks (game thread).
    static void OnHostAction(std::string_view name, const std::string& payload);
    static void OnHostExited();
    static void OnHostRefused();
    void OnAction(std::string_view name, const std::string& payload);

    void PublishMode();
    void PublishState(const nlohmann::json& state);
    void RepublishActivePopup();
    void CloseAll();
    void ShowLevelUpModalNow(const nlohmann::json& payload, bool whisperIfOff, std::uint32_t generation);
    void TryShowHead(bool force);
    // `pageClosedHead`: the page closed the head itself (its seq matched), so it goes whatever its state.
    void PopupDismissed(bool pageClosedHead = false);
    void LeaveEmptyPopup();
    void OnTick();
    void QueueChanged();                        // after any queue change: keeps the ticker and the menu flag in step
    static void TickerLoop(std::stop_token stop);

    // Whispers (game thread).
    static void OnWhisperDone(const std::string& payload);
    [[nodiscard]] bool CanShowWhispers() const;
    void ClearWhispers();                       // the ledger or a popup came up, whispers went off, or a load
    void WhispersChanged();                     // keeps the menu sink's flag and the ticker in step
    void NotifyWhisper(const romantasy::ui::whispers::Whisper& whisper);  // the game's own notification
    void OnWhisperCardLost();                   // the whisper view died: its whisper goes to the fallback
    // A tier whisper in place of a popup that is switched off or cannot be shown.
    void WhisperTier(const nlohmann::json& payload, bool isLoss, std::uint32_t generation);

    void ApplySettingsChange(const romantasy::ui::SettingsChange& change);
    // Bond and debug requests mutate the romance layer, so they are queued to
    // the main thread through the SKSE task interface; the outcome message
    // returns through SendState.
    void ApplyBondRequest(const romantasy::ui::BondRequest& request);
    void ApplyDebugRequest(const romantasy::ui::DebugRequest& request);
    // Reads the bond's journal on the main thread, as bond requests are run, and publishes it on romantasyJournal.
    void ApplyJournalRequest(const std::string& identity);
    void PlayModalMusic(bool isLoss);
    void StopModalMusic(std::uint16_t fadeTimeMs = 500);

    friend class RomantasyMenuSink;

    romantasy::ui::PopupQueue _queue;
    // The queue is not empty. Read by the menu sink without touching the queue.
    std::atomic<bool> _queuePending{ false };
    // Posts a tick to the game thread once a second while the queue is not empty or a whisper waits or is on the card.
    std::jthread _ticker;

    romantasy::ui::whispers::Pump _whispers;   // the queue and the whisper on the card
    std::atomic<std::uint32_t> _whisperGeneration{ 0 };
    bool _whispersEnabled = true;               // the value last given to the queue's SetEnabled
    // Read by the menu sink without touching the queue: a whisper waits or is on the card.
    std::atomic<bool> _whispersPending{ false };

    RE::BSSoundHandle _modalMusic;
    bool _modalMusicActive = false;
    bool _initialized = false;
    bool _developerToolsUnlocked = false;
    std::uint32_t _revision = 0;
};
