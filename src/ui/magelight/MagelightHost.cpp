#include "ui/magelight/MagelightHost.h"

#include "ui/magelight/Channels.h"
#include "ui/magelight/MagelightUI_API.h"

#include <filesystem>
#include <memory>
#include <utility>

namespace romantasy::ui
{
    namespace
    {
        using MAGELIGHT_API::Result;

        // The page lives in Magelight's mod tree, Data/Magelight/Romantasy/, resolved from the game's executable: the
        // working directory is not to be trusted.
        std::string PagePath()
        {
            wchar_t exe[MAX_PATH]{};
            if (!::GetModuleFileNameW(nullptr, exe, MAX_PATH)) return {};
            const auto page = std::filesystem::path(exe).parent_path() / L"Data" / L"Magelight" / L"Romantasy" / L"index.html";
            std::error_code error;
            if (!std::filesystem::exists(page, error)) return {};
            const auto utf8 = page.u8string();
            return std::string(utf8.begin(), utf8.end());
        }

        // The JSON text InteropCall hands the page. Game names arrive raw, so a byte that isn't valid UTF-8 becomes
        // U+FFFD instead of a throw that would end the publish (Tailor's SerializeTopic).
        std::string SerializeTopic(const nlohmann::json& value)
        {
            return value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        }

        // The open menus that pause the game, by name, for a log line: one of them is what refuses an entry.
        std::string PausingMenus()
        {
            std::string names;
            if (auto* ui = RE::UI::GetSingleton()) {
                for (const auto& entry : ui->menuMap) {
                    if (!entry.second.menu || !entry.second.menu->PausesGame()) continue;
                    if (!names.empty()) names += ", ";
                    names += entry.first.c_str() ? entry.first.c_str() : "?";
                }
            }
            return names.empty() ? "a menu" : names;
        }

        const char* ModeName(MagelightHost::Mode mode)
        {
            switch (mode) {
            case MagelightHost::Mode::Ledger: return "ledger";
            case MagelightHost::Mode::Popup:  return "popup";
            default:                          return "hidden";
            }
        }

        // A page action copied off Magelight's render thread for the game thread. The name points at the static
        // string in channels::kPageActions it was registered with.
        struct Action
        {
            const char* name;
            std::string payload;
        };
    }

    MagelightHost& MagelightHost::GetSingleton()
    {
        static MagelightHost host;
        return host;
    }

    void MagelightHost::Initialize(Callbacks callbacks)
    {
        if (_api) return;
        _callbacks = callbacks;
        using romantasy::health::HostState;
        const auto* api = MAGELIGHT_API::RequestApi4();
        if (!api) {
            logger::warn("MagelightHost: Magelight UI is not loaded; Romantasy's ledger cannot open");
            _hostState = HostState::Missing;
            return;
        }
        // Packed as MAJOR * 10000 + MINOR * 100 + PATCH, the number the version check below reads.
        _hostVersion = std::format("{}.{}.{}", api->hostVersionNumber / 10000, api->hostVersionNumber / 100 % 100,
            api->hostVersionNumber % 100);
        if (api->hostVersionNumber < kMinHostVersion) {
            logger::warn("MagelightHost: Magelight UI {} is older than 1.6.0; Romantasy's ledger cannot open", api->hostVersion);
            _hostState = HostState::TooOld;
            return;
        }
        const auto page = PagePath();
        if (page.empty()) {
            logger::error("MagelightHost: Data/Magelight/Romantasy/index.html is missing; reinstall Romantasy");
            _hostState = HostState::Failed;
            return;
        }

        MAGELIGHT_API::ModDesc mod{};
        mod.size = sizeof(mod);
        mod.modId = "Romantasy";
        mod.displayName = "Romantasy";
        mod.modVersion = MAGELIGHT_API::PackVersion(Plugin::VERSION.major(), Plugin::VERSION.minor(), Plugin::VERSION.patch());
        mod.minHostVersion = kMinHostVersion;
        mod.callbackThread = MAGELIGHT_API::CallbackThread::GameThread;
        mod.onEvent = &MagelightHost::OnHostEvent;
        MAGELIGHT_API::ModId modId = 0;
        if (const auto registered = api->RegisterMod(&mod, &modId); registered != Result::Ok) {
            logger::error("MagelightHost: Magelight refused to register Romantasy ({})", static_cast<std::int32_t>(registered));
            _hostState = HostState::Failed;
            return;
        }

        MAGELIGHT_API::ViewDesc desc{};
        desc.size = sizeof(desc);
        desc.name = "main";
        desc.htmlPath = page.c_str();
        desc.fullscreen = true;
        desc.layer = MAGELIGHT_API::Layer::Panel;
        MAGELIGHT_API::ViewId view = 0;
        if (const auto created = api->CreateViewEx(modId, &desc, &view); created != Result::Ok) {
            logger::error("MagelightHost: Magelight could not create Romantasy's view ({}): {}",
                static_cast<std::int32_t>(created), api->GetLastErrorMessage(modId));
            api->UnregisterMod(modId);
            _hostState = HostState::Failed;
            return;
        }
        _api = api;
        _hostState = HostState::Ok;
        _mod = modId;
        _view = view;

        for (const char* name : channels::kPageActions) {
            if (api->RegisterJSListenerEx(view, name, &MagelightHost::OnListener, const_cast<char*>(name)) != Result::Ok)
                logger::error("MagelightHost: Magelight refused listener {}: {}", name, api->GetLastErrorMessage(modId));
        }
        // The page closes its modals and the enrollment takeover on Escape, dismisses a popup with it, and asks for
        // the close itself; a gamepad's B reaches it the same way.
        api->SetEscapeCapture(view, true);
        // Painted while hidden, so the first open composites a finished page instead of painting it in that frame.
        if (api->QueryCapability("prepaint") == 1) api->SetViewPrepaint(view, true);
        // The pad is the page's while it holds UI mode; the page moves its own focus (view/js/pad.js).
        if (api->hostVersionNumber >= MAGELIGHT_API::PackVersion(1, 6, 0) && api->QueryCapability("gamepad") == 1) {
            MAGELIGHT_API::GamepadConfig pad{};
            pad.size = sizeof(pad);
            pad.enabled = true;
            pad.style = MAGELIGHT_API::kGamepadStyleNavigation;
            pad.allowCursor = true;
            if (const auto result = api->SetViewGamepad(view, &pad); result != Result::Ok)
                logger::warn("MagelightHost: Magelight refused the gamepad ({}): {}", static_cast<std::int32_t>(result),
                    api->GetLastErrorMessage(modId));
        }
        logger::info("MagelightHost: using Magelight UI {} ({})", api->hostVersion, page);
    }

    bool MagelightHost::HasHost() const
    {
        return _api && !_renderDead && !_pageFailed;
    }

    bool MagelightHost::CanEnter() const
    {
        if (!HasHost()) return false;
        if (_mode.load() != Mode::Hidden) return true;
        // Another mod's page holds UI mode: asking would only show our view, get Busy and hide it again.
        if (OtherModHoldsUIMode()) return false;
        auto* ui = RE::UI::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!ui || !player || !player->Is3DLoaded() || ui->GameIsPaused() ||
            ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) return false;
        return true;
    }

    std::string MagelightHost::EnterProblem() const
    {
        if (!HasHost()) return "no usable Magelight host";
        if (OtherModHoldsUIMode()) return std::format("another Magelight mod ({}) holds the cursor", _api->GetUIModeOwner());
        auto* ui = RE::UI::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!ui || !player) return "the game is not up yet";
        if (!player->Is3DLoaded()) return "the player is not loaded yet";
        if (ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))
            return "the main menu or a loading screen is open";
        if (ui->GameIsPaused()) return std::format("the game is paused by {}", PausingMenus());
        return "no reason found";
    }

    bool MagelightHost::OtherModHoldsUIMode() const
    {
        if (!_api) return false;
        const auto owner = _api->GetUIModeOwner();
        return owner != 0 && owner != _mod;
    }

    MagelightHost::Mode MagelightHost::CurrentMode() const
    {
        return _mode.load();
    }

    bool MagelightHost::Enter(Mode mode)
    {
        if (mode == Mode::Hidden) {
            Leave();
            return true;
        }
        if (!HasHost()) return false;
        const Mode current = _mode.load();
        if (current == mode) return true;
        const bool fresh = current == Mode::Hidden;
        if (fresh) {
            if (!CanEnter()) {
                logger::info("MagelightHost: the {} cannot open now: {}", ModeName(mode), EnterProblem());
                return false;
            }
            if (!_api->IsViewValid(_view)) {
                logger::warn("MagelightHost: Romantasy's view is gone; the {} cannot open", ModeName(mode));
                return false;
            }
            _api->ShowView(_view, true);
        }
        // No text-entry gate: Magelight types into the page from the window's own messages. Reclaim keeps the mode when
        // an entry follows the release of our own last close before that release has run (Ctrl+R opening the ledger
        // again, or the next popup right after the last one closed): without it the request is an in-place retarget
        // that no UIModeEntered confirms, and the posted exit then ends the mode under it. With no release of ours
        // pending the flag does nothing.
        std::uint32_t flags = MAGELIGHT_API::kUIModeFlagNoTextEntry | MAGELIGHT_API::kUIModeFlagReclaim;
        if (mode == Mode::Ledger) flags |= MAGELIGHT_API::kUIModeFlagPause;
        const auto requested = _api->RequestUIMode(_view, flags);
        if (requested == Result::Ok) {
            if (fresh) _entered = false;
            _mode = mode;
            logger::info("MagelightHost: {} requested ({})", ModeName(mode), fresh ? "opened" : "switched in place");
            return true;
        }
        if (fresh) _api->ShowView(_view, false);
        if (requested == Result::Busy)
            logger::info("MagelightHost: another Magelight page holds the cursor; the {} stays closed", ModeName(mode));
        else if (std::exchange(_refusalLogged, true))
            logger::debug("MagelightHost: Magelight refused the {} again ({}): {}", ModeName(mode), static_cast<std::int32_t>(requested),
                _api->GetLastErrorMessage(_mod));
        else
            logger::warn("MagelightHost: Magelight refused the {} ({}): {}", ModeName(mode), static_cast<std::int32_t>(requested),
                _api->GetLastErrorMessage(_mod));
        return false;
    }

    void MagelightHost::Leave()
    {
        if (_mode.load() == Mode::Hidden) return;
        TellPageHidden();
        _mode = Mode::Hidden;
        const bool held = std::exchange(_entered, false);
        if (!_api) return;
        // Withdraw an entry the host has not run yet; release one it has. A held mode is released even when a later
        // request on it (a switch between ledger and popup) was the one withdrawn.
        if (_api->CancelUIModeRequest(_mod) != Result::Ok || held) _api->ReleaseUIMode(_mod);
        _api->ShowView(_view, false);
        logger::info("MagelightHost: closed");
    }

    void MagelightHost::Publish(const char* topic, const nlohmann::json& value)
    {
        if (!_api || _mode.load() == Mode::Hidden) return;
        const auto text = SerializeTopic(value);
        _api->InteropCall(_view, topic, text.c_str());
    }

    void MagelightHost::TellPageHidden()
    {
        // Before the mode goes Hidden (Publish sends nothing after) and only to a live page. After a refused entry no
        // __uimode reaches the page, so this is the only word it gets that it is no longer on screen.
        if (HasHost()) Publish(channels::kMode, { { "mode", "hidden" } });
    }

    void MagelightHost::OnListener(std::uint64_t, const char* argument, void* user)
    {
        // Render or page thread, inside the frame: copy the argument and let the host post it to the game thread.
        // Posting to SKSE's task queue from here can hang.
        const auto* api = GetSingleton()._api;
        const auto* name = static_cast<const char*>(user);
        auto action = std::make_unique<Action>(Action{ name, argument ? argument : "" });
        if (api && api->PostGameTask(&MagelightHost::RunAction, action.get()) == Result::Ok)
            action.release();
        else
            logger::warn("MagelightHost: could not post page action {}", name ? name : "?");
    }

    void MagelightHost::RunAction(void* user)
    {
        const std::unique_ptr<Action> action(static_cast<Action*>(user));
        const auto& callbacks = GetSingleton()._callbacks;
        if (callbacks.onAction && action->name) callbacks.onAction(action->name, action->payload);
    }

    void MagelightHost::OnHostEvent(const MAGELIGHT_API::EventData* event, void*)
    {
        if (!event) return;
        auto& host = GetSingleton();
        if (host._callbacks.onEvent) host._callbacks.onEvent(event);
        host.OnEvent(event);
    }

    void MagelightHost::ExitedByHost(const char* why)
    {
        logger::info("MagelightHost: the {} ended: {}", ModeName(_mode.load()), why);
        TellPageHidden();
        _mode = Mode::Hidden;
        const bool held = std::exchange(_entered, false);
        if (_api) {
            // An exit the host made leaves the view drawn; a dead page or overlay may still hold the mode, and a
            // withdrawn switch between ledger and popup leaves the earlier mode held.
            if (_api->CancelUIModeRequest(_mod) != Result::Ok || held) _api->ReleaseUIMode(_mod);
            _api->ShowView(_view, false);
        }
        if (_callbacks.onExited) _callbacks.onExited();
    }

    // Game thread (ModDesc.callbackThread).
    void MagelightHost::OnEvent(const MAGELIGHT_API::EventData* event)
    {
        using MAGELIGHT_API::Event;
        const char* detail = event->detail && *event->detail ? event->detail : "";
        if (event->type == Event::RenderDead) {
            // Not view-specific: Magelight turned its overlay off for the session.
            _renderDead = true;
            _pageReady = false;
            logger::error("MagelightHost: Magelight turned its overlay off for this session; Romantasy's ledger cannot open");
            if (_mode.load() != Mode::Hidden) ExitedByHost("the overlay is off");
            return;
        }
        if (!_api || event->view != _view) return;
        switch (event->type) {
        case Event::UIModeEntered:
            if (_mode.load() == Mode::Hidden) {
                // An entry that lands after a close is given back, and the view it showed is hidden again.
                _api->ReleaseUIMode(_mod);
                _api->ShowView(_view, false);
                break;
            }
            _entered = true;
            _refusalLogged = false;
            break;
        case Event::UIModeRefused:
            // Logged at warn once until the next successful entry; a popup retries once a second.
            if (std::exchange(_refusalLogged, true))
                logger::debug("MagelightHost: Magelight refused the {} again: {}", ModeName(_mode.load()), *detail ? detail : "no reason given");
            else
                logger::warn("MagelightHost: Magelight refused the {}: {}", ModeName(_mode.load()), *detail ? detail : "no reason given");
            if (_mode.load() == Mode::Hidden) {
                _api->ShowView(_view, false);  // never left shown between retries
                break;
            }
            Leave();  // hides the view
            if (_callbacks.onRefused) _callbacks.onRefused();
            break;
        case Event::UIModeExited:
            if (_mode.load() == Mode::Hidden) break;
            if (!_entered) {
                // From a session before the current request; this one has not been entered yet.
                logger::info("MagelightHost: an earlier UI mode ended; the {} request stands", ModeName(_mode.load()));
                break;
            }
            ExitedByHost("UI mode ended outside Romantasy");
            break;
        case Event::FocusDenied:
            logger::info("MagelightHost: another mod asked for the cursor while Romantasy holds it");
            break;
        case Event::ViewLoadFailed:
            logger::error("MagelightHost: Magelight could not load Romantasy's page: {}", detail);
            _pageFailed = true;
            _pageReady = false;
            if (_mode.load() != Mode::Hidden) ExitedByHost("the page failed to load");
            break;
        case Event::ViewDestroyed:
            logger::error("MagelightHost: Romantasy's view was destroyed");
            _pageFailed = true;
            _pageReady = false;
            if (_mode.load() != Mode::Hidden) ExitedByHost("the view was destroyed");
            break;
        case Event::ConsoleMessage:
            if (event->x == 1) logger::warn("Romantasy page: {}", detail);
            if (event->x == 2) logger::error("Romantasy page: {} (line {})", detail, event->y);
            break;
        default:
            break;
        }
    }
}
