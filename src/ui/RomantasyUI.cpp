#include "ui/RomantasyUI.h"

#include "RE/B/BSAudioManager.h"
#include "RE/B/BSSoundHandle.h"
#include "RE/I/ID.h"

#include "health/HealthMonitor.h"
#include "romance/RomanceManager.h"
#include "settings/Settings.h"
#include "input/InputMode.h"
#include "ostim/OStimBridge.h"
#include "ui/magelight/Channels.h"
#include "ui/magelight/MagelightHost.h"
#include "ui/magelight/WhisperView.h"

#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

namespace
{
    using romantasy::ui::MagelightHost;
    using romantasy::ui::PopupQueue;
    using romantasy::ui::WhisperView;
    namespace channels = romantasy::ui::channels;
    namespace whispers = romantasy::ui::whispers;

    // whispers::MenuBlocks names these menus as text; they must stay the game's names.
    static_assert(RE::HUDMenu::MENU_NAME == "HUD Menu" && RE::FaderMenu::MENU_NAME == "Fader Menu" &&
                  RE::CursorMenu::MENU_NAME == "Cursor Menu" && RE::DialogueMenu::MENU_NAME == "Dialogue Menu" &&
                  RE::MainMenu::MENU_NAME == "Main Menu" && RE::LoadingMenu::MENU_NAME == "Loading Menu");

    constexpr std::string_view kLevelUpMusicPath = "Sound\\FX\\Romantasy\\levelup.wav";
    constexpr std::string_view kLevelDownMusicPath = "Sound\\FX\\Romantasy\\loselevel.wav";
    constexpr std::uint16_t kModalMusicFadeInMs = 350;
    constexpr std::uint32_t kModalMusicFlags = 0x1A;
    constexpr std::uint32_t kModalMusicPriority = 0x10;

    constexpr const char* kNeedsHost = "Romantasy's ledger needs Magelight UI 1.6 or newer.";
    constexpr const char* kOpenMessage = "Romance records synchronized.";

    // Validate the developer-tools confirmation phrase. The whole text is compared, embedded NULs included.
    bool CheckDeveloperPassword(std::string_view text) noexcept
    {
        return text == "darthmaul666";
    }

    // Page text that may go on to a game lookup: no NUL or other control character, which PageRequests lets through.
    bool IsPlainText(std::string_view text) noexcept
    {
        return std::none_of(text.begin(), text.end(), [](char c) {
            const auto byte = static_cast<unsigned char>(c);
            return byte < 0x20 || byte == 0x7F;
        });
    }

    bool IsLoss(const nlohmann::json& payload)
    {
        if (!payload.is_object()) {
            return false;
        }
        const auto it = payload.find("changeDirection");
        return it != payload.end() && it->is_string() && it->get_ref<const std::string&>() == "loss";
    }

    // A text field of a level-change payload; empty when it is missing or not text.
    std::string PayloadText(const nlohmann::json& payload, const char* key)
    {
        if (!payload.is_object()) {
            return {};
        }
        const auto it = payload.find(key);
        return it != payload.end() && it->is_string() ? it->get<std::string>() : std::string();
    }

    bool OnScreen(PopupQueue::Head head)
    {
        return head == PopupQueue::Head::Requested || head == PopupQueue::Head::Shown;
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

// A menu that closes may be the one that kept a waiting popup back. The sink only reads the atomic flag and posts; the
// queue itself is touched in the task, on the game thread.
class RomantasyMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
    static RomantasyMenuSink* GetSingleton()
    {
        static RomantasyMenuSink sink;
        return &sink;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (event && !event->opening && RomantasyUI::GetSingleton()._queuePending.load()) {
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([]() {
                    auto& ui = RomantasyUI::GetSingleton();
                    if (ui._queue.HeadState() == PopupQueue::Head::Waiting) {
                        ui.TryShowHead(true);
                    }
                });
            }
        }
        // Whispers: a menu that closes may be the one that held one back, and one that opens takes the card off the
        // screen. Every menu event while a whisper waits or is up is also the pump's regular beat, which ends one the
        // page never answered and lets old ones age out.
        if (event && RomantasyUI::GetSingleton()._whispersPending.load()) {
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([]() { RomantasyUI::GetSingleton().PumpWhispers(); });
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

RomantasyUI& RomantasyUI::GetSingleton()
{
    static RomantasyUI singleton;
    return singleton;
}

void RomantasyUI::Initialize()
{
    if (_initialized) {
        return;
    }
    _initialized = true;
    auto& host = MagelightHost::GetSingleton();
    host.Initialize({ &RomantasyUI::OnHostAction, &RomantasyUI::OnHostExited, &RomantasyUI::OnHostRefused,
        [](const MAGELIGHT_API::EventData* event) {
            auto& view = WhisperView::GetSingleton();
            const bool was = view.Available();
            view.OnEvent(event);
            if (was && !view.Available()) {
                RomantasyUI::GetSingleton().OnWhisperCardLost();  // the card's whisper is not lost with it
            }
        } });
    // The whisper card shares only the host's API and mod; without them whispers go to the game's notifications.
    auto& whisperView = WhisperView::GetSingleton();
    whisperView.Initialize(host.Api(), host.Mod(), &RomantasyUI::OnWhisperDone);
    const auto& settings = Settings::GetSingleton();
    whisperView.Place(settings.WhisperCorner());
    _whispersEnabled = settings.Whispers();
    _whispers.SetEnabled(_whispersEnabled);
    if (auto* ui = RE::UI::GetSingleton()) {
        ui->AddEventSink<RE::MenuOpenCloseEvent>(RomantasyMenuSink::GetSingleton());
    } else {
        logger::error("RomantasyUI: no UI singleton; waiting popups retry once a second only");
    }
    logger::info("Romantasy UI initialized (Magelight host {})", MagelightHost::GetSingleton().HasHost() ? "ready" : "unavailable");
}

void RomantasyUI::Toggle()
{
    auto& host = MagelightHost::GetSingleton();
    if (!host.HasHost()) {
        RE::SendHUDMessage::ShowHUDMessage(kNeedsHost);
        logger::warn("RomantasyUI: the ledger cannot open without Magelight UI 1.6 or newer");
        return;
    }
    const auto mode = host.CurrentMode();
    logger::info("RomantasyUI: toggle requested ({})", mode == MagelightHost::Mode::Ledger ? "ledger open" :
        mode == MagelightHost::Mode::Popup ? "popup open" : "closed");
    if (mode == MagelightHost::Mode::Ledger) {
        CloseAll();
        return;
    }
    if (!host.Enter(MagelightHost::Mode::Ledger)) {
        return;  // the host logged why
    }
    ClearWhispers();  // never shown over the ledger, and not shown stale after it
    PublishMode();
    PublishState(RomanceManager::GetSingleton().BuildStateJson(kOpenMessage));
    // A popup alone on screen carries on as an overlay (the page ignores a popup it already shows); one still waiting
    // shows over the ledger now.
    RepublishActivePopup();
    TryShowHead(true);
}

bool RomantasyUI::IsOpen() const
{
    return MagelightHost::GetSingleton().CurrentMode() == MagelightHost::Mode::Ledger;
}

void RomantasyUI::Close()
{
    if (MagelightHost::GetSingleton().CurrentMode() != MagelightHost::Mode::Hidden || _queue.Size() > 0) {
        logger::info("RomantasyUI: closing for a save load or new game ({} popup(s) dropped)", _queue.Size());
    }
    // A new game or a load: whispers of the old one are dropped, and any still on their way are stale.
    ++_whisperGeneration;
    ClearWhispers();
    CloseAll();
}

void RomantasyUI::CloseAll()
{
    MagelightHost::GetSingleton().Leave();
    _queue.Clear();
    StopModalMusic();
    QueueChanged();
    PumpWhispers();  // whispers raised while the ledger or a popup was up may show now
}

void RomantasyUI::SendState(std::string_view message)
{
    SKSE::GetTaskInterface()->AddTask([message = std::string(message)]() {
        auto& ui = RomantasyUI::GetSingleton();
        if (ui.IsOpen()) {
            ui.PublishState(RomanceManager::GetSingleton().BuildStateJson(message));
        }
    });
}

void RomantasyUI::SendState(const nlohmann::json& state)
{
    SKSE::GetTaskInterface()->AddTask([state]() {
        RomantasyUI::GetSingleton().PublishState(state);
    });
}

// The mode carries the comfort settings too: a popup shown before the ledger's first state must follow them.
void RomantasyUI::PublishMode()
{
    auto& host = MagelightHost::GetSingleton();
    const char* mode = nullptr;
    switch (host.CurrentMode()) {
    case MagelightHost::Mode::Ledger: mode = "ledger"; break;
    case MagelightHost::Mode::Popup:  mode = "popup"; break;
    default: return;
    }
    const auto& settings = Settings::GetSingleton();
    host.Publish(channels::kMode, {
        { "mode", mode },
        { "ledgerSize", std::string(settings.LedgerSize()) },
        { "highContrast", settings.HighContrast() },
        { "reduceMotion", settings.ReduceMotion() },
    });
}

void RomantasyUI::PublishState(const nlohmann::json& state)
{
    auto& host = MagelightHost::GetSingleton();
    if (host.CurrentMode() != MagelightHost::Mode::Ledger) {
        return;
    }
    const auto facts = romantasy::health::Gather();
    host.Publish(channels::kState, romantasy::ui::StampState(state, ++_revision, _developerToolsUnlocked,
        romantasy::health::ToJson(romantasy::health::BuildReport(facts), facts)));
}

void RomantasyUI::RepublishActivePopup()
{
    if (OnScreen(_queue.HeadState())) {
        MagelightHost::GetSingleton().Publish(channels::kLevelChange, *_queue.HeadPayload());
    }
}

void RomantasyUI::OnHostAction(std::string_view name, const std::string& payload)
{
    GetSingleton().OnAction(name, payload);
}

void RomantasyUI::OnHostExited()
{
    auto& ui = GetSingleton();
    ui.StopModalMusic();
    if (OnScreen(ui._queue.HeadState())) {
        logger::info("RomantasyUI: the page closed with a popup up; it is dropped");
        ui._queue.Dismiss();
    }
    ui.QueueChanged();  // waiting popups stay and are retried by the ticker
    ui.PumpWhispers();  // the view no longer blocks whispers
}

void RomantasyUI::OnHostRefused()
{
    auto& ui = GetSingleton();
    switch (ui._queue.HeadState()) {
    case PopupQueue::Head::Requested:
        ui._queue.MarkRefused();  // tried again when a menu closes, or within a second
        break;
    case PopupQueue::Head::Shown:
        // A switch to the ledger was refused and took the popup off screen with it.
        ui.StopModalMusic();
        ui._queue.Dismiss();
        break;
    default:
        break;
    }
    ui.QueueChanged();
    ui.PumpWhispers();  // the view is hidden again: whispers held back may show
}

void RomantasyUI::OnAction(std::string_view name, const std::string& payload)
{
    using namespace romantasy::ui;
    auto& host = MagelightHost::GetSingleton();
    // Any action proves the page's bridge is attached, so a lost first romantasyReady cannot hold popups back.
    host.MarkPageReady();  // waiting popups may be sent from now on (the ticker tries them)
    if (name == "romantasyReady") {
        const auto mode = host.CurrentMode();
        logger::info("RomantasyUI: the page is ready");
        if (mode == MagelightHost::Mode::Hidden) {
            return;
        }
        PublishMode();
        if (mode == MagelightHost::Mode::Ledger) {
            PublishState(RomanceManager::GetSingleton().BuildStateJson(kOpenMessage));
        }
        RepublishActivePopup();
    } else if (name == "romantasyOpen") {
        // Left Ctrl + R pressed over a popup alone: the page has the keyboard, so it asks for what the hotkey does.
        if (host.CurrentMode() == MagelightHost::Mode::Popup && !Settings::GetSingleton().OpenWithFavorites()) {
            Toggle();
        } else {
            logger::debug("RomantasyUI: ignored romantasyOpen (not a popup alone, or Favorites mode)");
        }
    } else if (name == "romantasyClose") {
        if (host.CurrentMode() != MagelightHost::Mode::Hidden) {
            CloseAll();
        }
    } else if (name == "romantasyRefresh") {
        PublishState(RomanceManager::GetSingleton().BuildStateJson("Ledger refreshed."));
    } else if (name == "romantasySettings") {
        if (const auto change = ParseSettingsChange(payload)) {
            ApplySettingsChange(*change);
        } else {
            logger::warn("RomantasyUI: ignored a malformed settings change");
        }
    } else if (name == "romantasyBond") {
        const auto request = ParseBondRequest(payload);
        const bool plain = request && IsPlainText(request->identity) &&
            std::all_of(request->preferences.begin(), request->preferences.end(),
                [](const auto& preference) { return IsPlainText(preference.first); });
        if (plain) {
            ApplyBondRequest(*request);
        } else {
            logger::warn("RomantasyUI: ignored a malformed bond request");
        }
    } else if (name == "romantasyJournalOpen") {
        if (const auto identity = ParseJournalRequest(payload)) {
            ApplyJournalRequest(*identity);
        } else {
            logger::warn("RomantasyUI: ignored a malformed journal request");
        }
    } else if (name == "romantasyDevUnlock") {
        if (const auto password = ParseDevUnlock(payload); password && CheckDeveloperPassword(*password)) {
            _developerToolsUnlocked = true;
            logger::info("RomantasyUI: developer tools unlocked");
        } else {
            logger::info("RomantasyUI: developer tools stay locked");
        }
        PublishState(RomanceManager::GetSingleton().BuildStateJson(""));
    } else if (name == "romantasyDevLock") {
        _developerToolsUnlocked = false;
        logger::info("RomantasyUI: developer tools locked");
        PublishState(RomanceManager::GetSingleton().BuildStateJson(""));
    } else if (name == "romantasyDebug") {
        if (const auto request = ParseDebugRequest(payload); request && IsPlainText(request->stat)) {
            ApplyDebugRequest(*request);
        } else {
            logger::warn("RomantasyUI: ignored a malformed debug request");
        }
    } else if (name == "romantasyPopupShown" || name == "romantasyPopupDismissed") {
        // A late message for a popup already timed out must not act on the next one: only the head's number counts.
        const auto seq = ParsePopupAck(payload);
        if (!seq) {
            logger::warn("RomantasyUI: ignored a malformed {}", name);
        } else if (_queue.HeadState() == PopupQueue::Head::None || *seq != _queue.HeadSeq()) {
            logger::debug("RomantasyUI: ignored {} for popup {} (head is {})", name, *seq, _queue.HeadSeq());
        } else if (name == "romantasyPopupDismissed") {
            // Also a head the host refused after the page drew it (Requested -> Waiting): the player has seen and
            // closed it, so it must not be published again.
            PopupDismissed(true);
        } else if (_queue.HeadState() == PopupQueue::Head::Requested) {
            _queue.MarkShown(PopupQueue::Clock::now());
            PlayModalMusic(IsLoss(*_queue.HeadPayload()));
        }
    } else {
        logger::warn("RomantasyUI: unknown page action {}", name);
    }
}

void RomantasyUI::ShowLevelUpModal(const nlohmann::json& payload, bool whisperIfOff, std::uint32_t generation)
{
    SKSE::GetTaskInterface()->AddTask([payload, whisperIfOff, generation]() {
        RomantasyUI::GetSingleton().ShowLevelUpModalNow(payload, whisperIfOff, generation);
    });
}

void RomantasyUI::ShowLevelUpModalNow(const nlohmann::json& payload, bool whisperIfOff, std::uint32_t generation)
{
    const bool isLoss = IsLoss(payload);
    const auto& settings = Settings::GetSingleton();
    if ((isLoss && !settings.ShowLossModals()) || (!isLoss && !settings.ShowGainModals())) {
        logger::debug("Romantasy level modal suppressed by settings ({})", isLoss ? "loss" : "gain");
        // The popup for this direction is off: a whisper says it instead, at the moment the popup would have come.
        if (whisperIfOff && settings.Whispers()) {
            WhisperTier(payload, isLoss, generation);
        }
        return;
    }
    if (!MagelightHost::GetSingleton().HasHost()) {
        logger::info("Romantasy level modal dropped; no usable Magelight host");
        // The popup cannot be shown at all: a whisper says it instead (through the game's notification).
        if (whisperIfOff && settings.Whispers()) {
            WhisperTier(payload, isLoss, generation);
        }
        return;
    }
    const auto seq = _queue.Push(payload, PopupQueue::Clock::now());
    logger::debug("Romantasy queued relationship level modal {}; {} in the queue", seq, _queue.Size());
    QueueChanged();
    TryShowHead(true);
}

void RomantasyUI::WhisperTier(const nlohmann::json& payload, bool isLoss, std::uint32_t generation)
{
    whispers::Whisper whisper;
    whisper.kind = isLoss ? whispers::Kind::TierLost : whispers::Kind::TierGained;
    whisper.name = PayloadText(payload, "followerName");
    whisper.label = PayloadText(payload, "levelName");
    Whisper(std::move(whisper), generation);
}

void RomantasyUI::TryShowHead(bool force)
{
    auto& host = MagelightHost::GetSingleton();
    // Before the page has said it is ready a popup would go unheard: it stays Waiting, bounded by kMaxWait.
    if (_queue.HeadState() != PopupQueue::Head::Waiting || !host.PageReady()) {
        return;
    }
    const auto now = PopupQueue::Clock::now();
    if (!_queue.ShouldAttempt(now, force)) {
        return;
    }
    if (host.CurrentMode() == MagelightHost::Mode::Hidden) {
        if (!host.CanEnter() || !host.Enter(MagelightHost::Mode::Popup)) {
            return;  // waits for a menu to close, or the next tick
        }
        ClearWhispers();  // never shown over a tier popup
        PublishMode();
    }
    host.Publish(channels::kLevelChange, *_queue.HeadPayload());
    _queue.MarkRequested(now);
}

void RomantasyUI::HandleLevelUpDismissed()
{
    PopupDismissed();
}

void RomantasyUI::PopupDismissed(bool pageClosedHead)
{
    StopModalMusic();
    if (pageClosedHead || OnScreen(_queue.HeadState())) {
        _queue.Dismiss();
    } else {
        logger::info("RomantasyUI: a popup dismissal with none on screen; ignored");
    }
    TryShowHead(true);
    LeaveEmptyPopup();
    QueueChanged();
}

void RomantasyUI::LeaveEmptyPopup()
{
    auto& host = MagelightHost::GetSingleton();
    if (host.CurrentMode() != MagelightHost::Mode::Popup || OnScreen(_queue.HeadState())) {
        return;
    }
    StopModalMusic();
    host.Leave();
    PumpWhispers();  // the popup no longer blocks whispers
}

void RomantasyUI::OnTick()
{
    // Also the whispers' beat while one waits or is on the card: the 8 second rule and the 30 second age never
    // wait for a menu event.
    PumpWhispers();
    if (_queue.Size() == 0) {
        LeaveEmptyPopup();
        QueueChanged();
        return;
    }
    auto& host = MagelightHost::GetSingleton();
    if (!host.HasHost()) {
        logger::warn("RomantasyUI: the Magelight host is gone; {} popup(s) dropped", _queue.Size());
        _queue.Clear();
        StopModalMusic();
        host.Leave();
        QueueChanged();
        return;
    }
    const auto now = PopupQueue::Clock::now();
    for (auto dropped = _queue.DropExpired(now); dropped > 0; --dropped) {
        logger::warn("RomantasyUI: a level-change popup waited over {} s and was dropped", PopupQueue::kMaxWait.count());
    }
    if (const auto dropped = _queue.DropAllIfShownTimedOut(now); dropped > 0) {
        // Sent to the page, never reported on screen (its music never started): the page is not answering, so the
        // rest would only hold the controls for the same timeout each. No other head is tried in this tick.
        logger::warn("RomantasyUI: the page did not show a popup within {} s; {} popup(s) dropped", PopupQueue::kShownTimeout.count(), dropped);
        if (host.CurrentMode() == MagelightHost::Mode::Popup) {
            host.Leave();  // an open ledger stays
        }
        QueueChanged();
        return;
    }
    if (_queue.ShownTooLong(now)) {
        logger::warn("RomantasyUI: a popup stayed up over {} s; treated as dismissed", PopupQueue::kMaxShown.count());
        PopupDismissed();
    }
    TryShowHead(false);
    LeaveEmptyPopup();
    QueueChanged();
}

void RomantasyUI::QueueChanged()
{
    const bool pending = _queue.Size() > 0;
    _queuePending = pending;
    if (pending || _whispersPending.load()) {
        if (!_ticker.joinable() || _ticker.get_stop_token().stop_requested()) {
            // A stopped ticker is let go, never joined here: this runs inside an SKSE task, and SKSE holds its task
            // lock while tasks run, so joining a ticker that is waiting in AddTask would deadlock.
            if (_ticker.joinable()) {
                _ticker.detach();
            }
            _ticker = std::jthread(&RomantasyUI::TickerLoop);
        }
    } else if (_ticker.joinable()) {
        _ticker.request_stop();
    }
}

void RomantasyUI::TickerLoop(std::stop_token stop)
{
    std::mutex mutex;
    std::condition_variable_any wake;
    std::unique_lock lock(mutex);
    // wait_for returns early, true, when stop is requested; otherwise false after a second.
    while (!wake.wait_for(lock, stop, std::chrono::seconds(1), [&stop] { return stop.stop_requested(); })) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            return;
        }
        tasks->AddTask([]() { RomantasyUI::GetSingleton().OnTick(); });
    }
}

void RomantasyUI::Whisper(romantasy::ui::whispers::Whisper whisper, std::uint32_t generation)
{
    if (generation != _whisperGeneration.load()) {
        logger::debug("RomantasyUI: dropped a whisper raised before the last load");
        return;
    }
    _whispers.Push(std::move(whisper), GetTickCount64());
    PumpWhispers();
}

bool RomantasyUI::CanShowWhispers() const
{
    whispers::ShowFacts facts;
    auto* ui = RE::UI::GetSingleton();
    auto* player = RE::PlayerCharacter::GetSingleton();
    facts.worldReady = ui && player && player->Is3DLoaded();
    if (ui) {
        facts.paused = ui->GameIsPaused();
        facts.inDialogue = ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
        for (const auto& [name, entry] : ui->menuMap) {
            const auto& menu = entry.menu;
            if (!menu || !menu->OnStack()) {
                continue;
            }
            const whispers::MenuFacts open{
                .name = name.c_str() ? std::string_view(name.c_str()) : std::string_view(),
                .pausesGame = menu->PausesGame(),
                .usesCursor = menu->UsesCursor(),
                .usesMenuContext = menu->UsesMenuContext(),
                .modal = menu->Modal(),
            };
            if (whispers::MenuBlocks(open)) {
                facts.menuOpen = true;
                break;
            }
        }
    }
    facts.ownViewVisible = MagelightHost::GetSingleton().CurrentMode() != MagelightHost::Mode::Hidden;
    facts.inScene = OStimBridge::GetSingleton().InScene() && Settings::GetSingleton().OStimHold();
    return whispers::CanShow(facts);
}

void RomantasyUI::PumpWhispers()
{
    using What = whispers::Pump::Decision::What;
    auto& view = WhisperView::GetSingleton();
    const bool canShow = CanShowWhispers();
    // One clock for every queue call: GetTickCount64, read here.
    const auto now = GetTickCount64();
    for (;;) {
        auto decision = _whispers.Next(now, canShow, view.Available());
        if (decision.timedOut != 0 && decision.timedOutBlocked) {
            logger::debug("RomantasyUI: whisper {} was hidden by a menu and not answered within {} s; taken down",
                decision.timedOut, whispers::kAnswerMs / 1000);
        } else if (decision.timedOut != 0) {
            logger::warn("RomantasyUI: the whisper page did not answer whisper {} within {} s; the card is taken down",
                decision.timedOut, whispers::kAnswerMs / 1000);
        }
        if (decision.gaveUp) {
            logger::warn("RomantasyUI: the whisper card did not answer {} times in a row; whispers use the game's notifications for this session",
                whispers::Pump::kCardFailures);
            view.GiveUp();
        }
        if (decision.what == What::Show) {
            // The comfort settings, read now: the card follows them from the next whisper on.
            const auto& settings = Settings::GetSingleton();
            const auto comfort = whispers::ComfortFields(settings.HighContrast(), settings.ReduceMotion());
            if (view.Show(decision.whisper, decision.seq, comfort)) {
                logger::debug("RomantasyUI: whisper {} on the card: {} ({})", decision.seq,
                    whispers::Describe(decision.whisper).name, whispers::KindName(decision.whisper.kind));
                break;
            }
            // The card could not take it (the display size not known yet): the game's notification instead.
            _whispers.ShowFailed();
        } else if (decision.what == What::None) {
            if (decision.hide) {
                view.Hide();
            }
            break;
        }
        NotifyWhisper(decision.whisper);
    }
    WhispersChanged();
}

void RomantasyUI::NotifyWhisper(const romantasy::ui::whispers::Whisper& whisper)
{
    logger::debug("RomantasyUI: whisper to the game's notification: {} ({})", whispers::Describe(whisper).name,
        whispers::KindName(whisper.kind));
    const auto text = whispers::NotificationMarkup(whisper);
    RE::SendHUDMessage::ShowHUDMessage(text.c_str());
}

void RomantasyUI::OnWhisperCardLost()
{
    if (const auto lost = _whispers.Lost()) {
        NotifyWhisper(*lost);
    }
    PumpWhispers();  // the waiting ones follow through the fallback
}

void RomantasyUI::OnWhisperDone(const std::string& payload)
{
    auto& ui = GetSingleton();
    // A late answer for a whisper already timed out must not end the next one: only the card's own seq counts.
    if (const auto seq = romantasy::ui::ParsePopupAck(payload); !seq) {
        logger::debug("RomantasyUI: ignored a malformed whisper answer");
    } else if (!ui._whispers.Answer(*seq)) {
        logger::debug("RomantasyUI: ignored the answer for whisper {} (on the card: {})", *seq, ui._whispers.Current());
    }
    ui.PumpWhispers();  // shows the next, or hides the card
}

void RomantasyUI::ClearWhispers()
{
    _whispers.Clear();
    WhisperView::GetSingleton().Hide();
    WhispersChanged();
}

void RomantasyUI::WhispersChanged()
{
    _whispersPending = _whispers.Pending();
    QueueChanged();  // the ticker runs while a whisper waits or is on the card
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
    if (change.detailedLog) {
        settings.SetDetailedLog(*change.detailedLog);
        settings.ApplyLogLevel();
    }
    if (change.pace) settings.SetPace(*change.pace);
    if (change.tierMoment) settings.SetTierMoment(*change.tierMoment);
    if (change.whispers) settings.SetWhispers(*change.whispers);
    if (change.whisperCorner) settings.SetWhisperCorner(*change.whisperCorner);
    if (change.rosterOrder) settings.SetRosterOrder(*change.rosterOrder);
    if (change.ledgerSize) settings.SetLedgerSize(*change.ledgerSize);
    if (change.highContrast) settings.SetHighContrast(*change.highContrast);
    if (change.reduceMotion) settings.SetReduceMotion(*change.reduceMotion);
    if (change.skyrimNetBridge) settings.SetSkyrimNetBridge(*change.skyrimNetBridge);
    if (change.skyrimNetConversation) settings.SetSkyrimNetConversation(*change.skyrimNetConversation);
    if (change.ostimMoments) settings.SetOStimMoments(*change.ostimMoments);
    if (change.ostimHold) settings.SetOStimHold(*change.ostimHold);
    settings.Save();
    // Only a real change restarts the queue (SetEnabled starts it clean); off also takes the card down.
    if (settings.Whispers() != _whispersEnabled) {
        _whispersEnabled = settings.Whispers();
        _whispers.SetEnabled(_whispersEnabled);
        if (!_whispersEnabled) {
            ClearWhispers();
        }
    }
    if (change.whisperCorner) {
        WhisperView::GetSingleton().Place(settings.WhisperCorner());  // the next card, and the one up now
    }
    if (favoritesChanged) {
        InputMode::ApplyForLoadedGame();  // grants/removes the power and (un)registers Ctrl+R
    }
    logger::info("RomantasyUI: dashboard settings updated (favorites={}, gain={}, loss={}, away={}, detailed log={}, pace={}, tier moment={}, whispers={}, whisper corner={}, roster order={}, ledger size={}, high contrast={}, reduce motion={}, SkyrimNet bridge={}, SkyrimNet conversation={}, OStim moments={}, OStim hold={})",
        settings.OpenWithFavorites(), settings.ShowGainModals(), settings.ShowLossModals(), settings.ShowAwayFollowers(),
        settings.DetailedLog(), romantasy::pacing::Key(settings.Pace()), romantasy::pacing::Key(settings.TierMoment()),
        settings.Whispers(), romantasy::ui::whispers::Key(settings.WhisperCorner()),
        settings.RosterOrder(), settings.LedgerSize(), settings.HighContrast(), settings.ReduceMotion(),
        settings.SkyrimNetBridge(), settings.SkyrimNetConversation(), settings.OStimMoments(), settings.OStimHold());
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

void RomantasyUI::ApplyJournalRequest(const std::string& identity)
{
    SKSE::GetTaskInterface()->AddTask([identity]() {
        const auto journal = RomanceManager::GetSingleton().BuildJournalJson(identity);
        logger::debug("RomantasyUI: journal request for {} -> found={}", identity, journal.value("found", false));
        auto& host = MagelightHost::GetSingleton();
        if (host.CurrentMode() == MagelightHost::Mode::Ledger) {
            host.Publish(channels::kJournal, journal);
        }
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
    logger::debug("Romantasy modal music started: {}", path);
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
