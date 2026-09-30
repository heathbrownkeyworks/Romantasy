#include "ui/RomantasyUI.h"

#include "RE/B/BSAudioManager.h"
#include "RE/B/BSSoundHandle.h"
#include "RE/C/ControlMap.h"
#include "RE/I/ID.h"
#include "RE/U/UIMessageQueue.h"

#include "romance/RomanceManager.h"
#include "settings/Settings.h"
#include "input/InputMode.h"
#include "ui/ImGuiHost.h"
#include "ui/menus/LedgerMenu.h"
#include "ui/menus/PopupMenu.h"

#include <cstdlib>
#include <unordered_map>

namespace
{
    constexpr std::string_view kLevelUpMusicPath = "Sound\\FX\\Romantasy\\levelup.wav";
    constexpr std::string_view kLevelDownMusicPath = "Sound\\FX\\Romantasy\\loselevel.wav";
    constexpr std::uint16_t kModalMusicFadeInMs = 350;
    constexpr std::uint32_t kModalMusicFlags = 0x1A;
    constexpr std::uint32_t kModalMusicPriority = 0x10;

    std::int64_t SteadyNowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // Hex FormID text -> the actor (reference first, then the base NPC's unique actor).
    RE::Actor* ResolveIdentityActor(const std::string& identityText)
    {
        if (identityText.empty()) {
            return nullptr;
        }
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(identityText.c_str(), &end, 16);
        if (*end != '\0' || parsed == 0 || parsed > 0xFFFFFFFFul) {
            return nullptr;
        }
        const auto formID = static_cast<RE::FormID>(parsed);
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID)) {
            return actor;
        }
        if (auto* baseNpc = RE::TESForm::LookupByID<RE::TESNPC>(formID)) {
            return baseNpc->GetUniqueActor();
        }
        return nullptr;
    }

    const char* BondOutcomeMessage(romantasy::ui::BondRequest::Kind kind, bool ok)
    {
        using Kind = romantasy::ui::BondRequest::Kind;
        switch (kind) {
        case Kind::Enroll:  return ok ? "A new bond was entered in the ledger." : "That companion could not be added.";
        case Kind::Replace: return ok ? "Companion personality sealed." : "The personality could not be saved. Your previous preferences are unchanged.";
        case Kind::Reset:   return ok ? "Player-created bond reset." : "That bond cannot be reset here.";
        case Kind::Remove:  return ok ? "Player-created bond removed." : "The bond could not be removed. Your existing bond is unchanged.";
        }
        return ok ? "Ledger updated." : "That request was refused.";
    }

    const char* BondKindName(romantasy::ui::BondRequest::Kind kind)
    {
        using Kind = romantasy::ui::BondRequest::Kind;
        switch (kind) {
        case Kind::Enroll:  return "enroll";
        case Kind::Replace: return "replace";
        case Kind::Reset:   return "reset";
        case Kind::Remove:  return "remove";
        }
        return "unknown";
    }
}

RomantasyUI& RomantasyUI::GetSingleton()
{
    static RomantasyUI singleton;
    return singleton;
}

void RomantasyUI::Initialize()
{
    if (_registered) {
        return;
    }
    if (REL::Module::IsVR()) {
        logger::warn("RomantasyUI: the ImGui dashboard is not supported on Skyrim VR; core tracking continues");
        return;
    }
    _registered = LedgerMenu::Register() && PopupMenu::Register();
    if (!_registered) {
        logger::error("RomantasyUI: menu registration failed; the dashboard is disabled for this session");
        return;
    }
    logger::info("Romantasy UI initialized (ImGui menus registered; renderer attaches on first open)");
}

void RomantasyUI::Toggle()
{
    if (!_registered) {
        logger::warn("RomantasyUI::Toggle called before UI initialization");
        return;
    }
    if (_ledgerPending.load() || _closePending.load()) {
        const std::int64_t waitedMs = SteadyNowMs() - _pendingSinceMs.load(std::memory_order_acquire);
        if (waitedMs > 2000) {
            logger::warn("RomantasyUI: stale menu request ({} s); resetting", waitedMs / 1000);
            _ledgerPending = false;
            _closePending = false;
        }
    }
    logger::info("RomantasyUI: toggle requested (open={}, pending={})", _isOpen.load(), _ledgerPending.load());
    if (_isOpen.load() || _ledgerPending.load()) {
        RequestClose();
        return;
    }
    _ledgerPending = true;
    _pendingSinceMs.store(SteadyNowMs(), std::memory_order_release);
    if (_popupMenuOpen.load()) {
        // The active popup carries on as an overlay inside the ledger.
        ShowMenu(PopupMenu::MENU_NAME, false);
    }
    ShowMenu(LedgerMenu::MENU_NAME, true);
}

bool RomantasyUI::IsOpen() const
{
    return _isOpen.load();
}

void RomantasyUI::RequestClose()
{
    if (_closePending.load()) {
        return;
    }
    if (!_isOpen.load() && !_ledgerPending.load()) {
        return;
    }
    _closePending = true;
    _pendingSinceMs.store(SteadyNowMs(), std::memory_order_release);
    ShowMenu(LedgerMenu::MENU_NAME, false);
}

void RomantasyUI::RequestPopupDismiss()
{
    if (_activePopup) {
        _popupView.Dismiss();
    }
}

void RomantasyUI::OnLedgerShown()
{
    logger::info("RomantasyUI: ledger shown");
    _isOpen = true;
    _ledgerPending = false;
    _closePending = false;
    if (!_textInputAllowed) {
        if (auto* controlMap = RE::ControlMap::GetSingleton()) {
            controlMap->AllowTextInput(true);
            _textInputAllowed = true;
            logger::info("RomantasyUI: text input enabled");
        }
    }
    auto& host = ImGuiHost::GetSingleton();
    host.SetActive(true);
    host.ClearInput();
    _ledgerView.OnOpen();
    SendState(RomanceManager::GetSingleton().BuildStateJson("Romance records synchronized."));
    PullPendingState();
}

void RomantasyUI::OnLedgerHidden()
{
    logger::info("RomantasyUI: ledger hidden");
    _isOpen = false;
    _ledgerPending = false;
    _closePending = false;
    if (_textInputAllowed) {
        if (auto* controlMap = RE::ControlMap::GetSingleton()) {
            controlMap->AllowTextInput(false);
        }
        _textInputAllowed = false;
        logger::info("RomantasyUI: text input disabled");
    }
    _activePopup.reset();
    _levelUpQueue.clear();
    StopModalMusic();
    auto& host = ImGuiHost::GetSingleton();
    host.ClearInput();
    if (!_popupMenuOpen.load()) {
        host.SetActive(false);
    }
}

void RomantasyUI::OnPopupShown()
{
    logger::info("RomantasyUI: popup menu shown");
    _popupMenuOpen = true;
    auto& host = ImGuiHost::GetSingleton();
    host.SetActive(true);
    host.ClearInput();
}

void RomantasyUI::OnPopupHidden()
{
    logger::info("RomantasyUI: popup menu hidden");
    _popupMenuOpen = false;
    if (_isOpen.load() || _ledgerPending.load()) {
        return;  // Toggle hand-off: the popup continues as an overlay inside the ledger
    }
    // The menu went away without the ledger taking over (force-hide, or a
    // hide we requested): drop the popup state and silence the music.
    _activePopup.reset();
    _levelUpQueue.clear();
    StopModalMusic();
    ImGuiHost::GetSingleton().SetActive(false);
}

void RomantasyUI::SendState(std::string_view message)
{
    std::scoped_lock lock(_stateLock);
    _pendingMessage = std::string(message);
}

void RomantasyUI::SendState(const nlohmann::json& state)
{
    auto parsed = romantasy::ui::UiStateFromJson(state);
    std::scoped_lock lock(_stateLock);
    _pendingState = std::move(parsed);
    _pendingMessage.reset();
}

void RomantasyUI::PullPendingState()
{
    std::scoped_lock lock(_stateLock);
    bool applied = false;
    if (_pendingState) {
        _liveState = std::move(*_pendingState);
        _pendingState.reset();
        applied = true;
    }
    if (_pendingMessage) {
        _liveState.message = std::move(*_pendingMessage);
        _pendingMessage.reset();
        applied = true;
    }
    if (applied) {
        ++_revision;
    }
    // Facade-owned fields: a fresh parse resets them, so stamp them every pull.
    _liveState.revision = _revision;
    _liveState.developerToolsUnlocked = _developerToolsUnlocked;
}

void RomantasyUI::ShowLevelUpModal(const nlohmann::json& payload)
{
    if (!_registered) {
        return;
    }
    DropUndrawnPopup();
    const auto change = romantasy::ui::LevelChangeFromJson(payload);
    const auto& settings = Settings::GetSingleton();
    if ((change.isLoss && !settings.ShowLossModals()) || (!change.isLoss && !settings.ShowGainModals())) {
        logger::info("Romantasy level modal suppressed by settings ({})", change.isLoss ? "loss" : "gain");
        return;
    }
    if (_activePopup) {
        _levelUpQueue.push_back(change);
        logger::info("Romantasy queued relationship level modal; {} modal(s) waiting", _levelUpQueue.size());
        return;
    }
    ShowLevelUpModalNow(change);
}

void RomantasyUI::ShowLevelUpModalNow(const romantasy::ui::LevelChange& change)
{
    _activePopup = change;
    _popupView.Reset();
    _popupDrawn = false;
    _popupRequestedAt = std::chrono::steady_clock::now();
    if (!_isOpen.load() && !_ledgerPending.load() && !_popupMenuOpen.load()) {
        ShowMenu(PopupMenu::MENU_NAME, true);
    }
}

void RomantasyUI::HandleLevelUpDismissed()
{
    _activePopup.reset();
    StopModalMusic();
    if (!_levelUpQueue.empty()) {
        const auto next = std::move(_levelUpQueue.front());
        _levelUpQueue.pop_front();
        ShowLevelUpModalNow(next);
        return;
    }
    if (_popupMenuOpen.load() && !_isOpen.load()) {
        ShowMenu(PopupMenu::MENU_NAME, false);
    }
}

void RomantasyUI::DrawLedgerFrame()
{
    auto& host = ImGuiHost::GetSingleton();
    if (!host.BeginFrame()) {
        RequestClose();  // renderer unavailable: never leave an invisible paused menu open
        return;
    }
    PullPendingState();
    const float dt = host.DeltaTime();
    const auto canvas = host.Canvas();
    const bool popupActive = _activePopup.has_value();

    const auto ledger = romantasy::ui::DrawLedger(_liveState, _ledgerView, host.Fonts(), canvas, dt, !popupActive, host.UsingGamepad());
    bool popupFinished = false;
    if (popupActive) {
        if (!_popupDrawn) {
            _popupDrawn = true;
            PlayModalMusic(_activePopup->isLoss);
        }
        popupFinished = romantasy::ui::DrawLevelUpPopup(*_activePopup, _popupView, host.Fonts(), canvas, dt, true, true, host.UsingGamepad()).finished;
    }
    host.EndFrame();

    if (popupFinished) {
        HandleLevelUpDismissed();
    } else if (!popupActive && ledger.closeRequested) {
        RequestClose();
    }
    if (ledger.refreshRequested) {
        SendState(RomanceManager::GetSingleton().BuildStateJson("Ledger refreshed."));
    }
    if (ledger.settings) {
        ApplySettingsChange(*ledger.settings);
    }
    if (ledger.developerTools) {
        ApplyDeveloperTools(*ledger.developerTools);
    }
    if (ledger.bond) {
        ApplyBondRequest(*ledger.bond);
    }
    if (ledger.debug) {
        ApplyDebugRequest(*ledger.debug);
    }
}

void RomantasyUI::ApplySettingsChange(const romantasy::ui::SettingsChange& change)
{
    auto& settings = Settings::GetSingleton();
    bool favoritesChanged = false;
    if (change.openWithFavorites) {
        favoritesChanged = settings.OpenWithFavorites() != *change.openWithFavorites;
        settings.SetOpenWithFavorites(*change.openWithFavorites);
    }
    if (change.showGainModals) settings.SetShowGainModals(*change.showGainModals);
    if (change.showLossModals) settings.SetShowLossModals(*change.showLossModals);
    if (change.showAwayFollowers) settings.SetShowAwayFollowers(*change.showAwayFollowers);
    settings.Save();
    if (favoritesChanged) {
        InputMode::ApplyForLoadedGame();  // grants/removes the power and (un)registers Ctrl+R
    }
    logger::info("RomantasyUI: dashboard settings updated (favorites={}, gain={}, loss={}, away={})",
        settings.OpenWithFavorites(), settings.ShowGainModals(), settings.ShowLossModals(), settings.ShowAwayFollowers());
    SendState(RomanceManager::GetSingleton().BuildStateJson("Preferences noted."));
}

void RomantasyUI::ApplyBondRequest(const romantasy::ui::BondRequest& request)
{
    using Kind = romantasy::ui::BondRequest::Kind;
    SKSE::GetTaskInterface()->AddTask([request]() {
        auto& manager = RomanceManager::GetSingleton();
        bool ok = false;
        const char* outcome = "no actor";
        if (auto* actor = ResolveIdentityActor(request.identity)) {
            std::unordered_map<std::string, std::int32_t> preferences;
            for (const auto& [editorID, direction] : request.preferences) {
                preferences.emplace(editorID, direction);
            }
            switch (request.kind) {
            case Kind::Enroll:  ok = manager.EnrollPlayerFollower(actor, preferences); break;
            case Kind::Replace: ok = manager.ReplacePlayerPreferences(actor, preferences); break;
            case Kind::Reset:   ok = manager.ResetPlayerFollowerPoints(actor); break;
            case Kind::Remove:  ok = manager.RemovePlayerFollower(actor); break;
            }
            outcome = ok ? "ok" : "refused";
        }
        logger::info("RomantasyUI: bond request {} for {} -> {}", BondKindName(request.kind), request.identity, outcome);
        RomantasyUI::GetSingleton().SendState(manager.BuildStateJson(BondOutcomeMessage(request.kind, ok)));
    });
}

void RomantasyUI::ApplyDebugRequest(const romantasy::ui::DebugRequest& request)
{
    if (!_developerToolsUnlocked) {
        logger::warn("Romantasy debug request blocked; developer tools are locked");
        return;
    }
    SKSE::GetTaskInterface()->AddTask([request]() {
        if (request.points) {
            RomanceManager::GetSingleton().DebugAddPoints(*request.points);
        } else if (!request.stat.empty()) {
            RomanceManager::GetSingleton().DebugApplyStat(request.stat, request.delta);
        }
    });
}

void RomantasyUI::ApplyDeveloperTools(bool unlocked)
{
    _developerToolsUnlocked = unlocked;
    logger::info("RomantasyUI: developer tools {}", unlocked ? "unlocked" : "locked");
}

void RomantasyUI::DrawPopupFrame()
{
    if (_isOpen.load()) {
        return;  // the ledger draws the popup as an overlay while it is open
    }
    if (!_activePopup) {
        // Nothing to show (a show that raced a Toggle, or a popup already
        // dropped): never leave an invisible menu holding the input context.
        ShowMenu(PopupMenu::MENU_NAME, false);
        return;
    }
    auto& host = ImGuiHost::GetSingleton();
    if (!host.BeginFrame()) {
        // Renderer unavailable: drop the popup rather than hold the menu context forever.
        _activePopup.reset();
        _levelUpQueue.clear();
        StopModalMusic();
        ShowMenu(PopupMenu::MENU_NAME, false);
        return;
    }
    if (!_popupDrawn) {
        _popupDrawn = true;
        PlayModalMusic(_activePopup->isLoss);
    }
    const bool finished = romantasy::ui::DrawLevelUpPopup(*_activePopup, _popupView, host.Fonts(), host.Canvas(), host.DeltaTime(), true, false, host.UsingGamepad()).finished;
    host.EndFrame();
    if (finished) {
        HandleLevelUpDismissed();
    }
}

void RomantasyUI::DropUndrawnPopup()
{
    if (!_activePopup || _popupDrawn) {
        return;
    }
    if (std::chrono::steady_clock::now() - _popupRequestedAt <= std::chrono::seconds(30)) {
        return;
    }
    logger::warn("RomantasyUI: popup never drew in 30 s; dropping it and its queue");
    _activePopup.reset();
    _levelUpQueue.clear();
    StopModalMusic();
    if (_popupMenuOpen.load()) {
        ShowMenu(PopupMenu::MENU_NAME, false);
    }
}

void RomantasyUI::ShowMenu(std::string_view menuName, bool show)
{
    const std::string name(menuName);
    SKSE::GetTaskInterface()->AddUITask([name, show]() {
        auto* queue = RE::UIMessageQueue::GetSingleton();
        if (!queue) {
            logger::warn("RomantasyUI: UI message queue unavailable; cannot {} {}", show ? "show" : "hide", name);
            return;
        }
        queue->AddMessage(RE::BSFixedString(name.c_str()), show ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide, nullptr);
    });
}

void RomantasyUI::PlayModalMusic(bool isLoss)
{
    StopModalMusic(180);

    auto* audioManager = RE::BSAudioManager::GetSingleton();
    if (!audioManager) {
        logger::warn("Romantasy modal music could not play; BSAudioManager is unavailable");
        return;
    }

    const auto path = isLoss ? kLevelDownMusicPath : kLevelUpMusicPath;
    RE::BSResource::ID fileID;
    fileID.GenerateFromPath(path.data());

    _modalMusic = RE::BSSoundHandle{};
    audioManager->GetSoundHandleByFile(_modalMusic, fileID, kModalMusicFlags, kModalMusicPriority);
    if (!_modalMusic.IsValid()) {
        logger::warn("Romantasy modal music handle was invalid for {}", path);
        return;
    }

    _modalMusic.SetVolume(1.0F);
    if (!_modalMusic.FadeInPlay(kModalMusicFadeInMs) && !_modalMusic.Play()) {
        logger::warn("Romantasy modal music failed to start for {}", path);
        _modalMusic = RE::BSSoundHandle{};
        return;
    }

    _modalMusicActive = true;
    logger::info("Romantasy modal music started: {}", path);
}

void RomantasyUI::StopModalMusic(std::uint16_t fadeTimeMs)
{
    if (!_modalMusicActive && !_modalMusic.IsValid()) {
        return;
    }

    if (_modalMusic.IsValid()) {
        if (fadeTimeMs > 0) {
            _modalMusic.FadeOutAndRelease(fadeTimeMs);
        } else {
            _modalMusic.Stop();
        }
    }

    _modalMusic = RE::BSSoundHandle{};
    _modalMusicActive = false;
}
