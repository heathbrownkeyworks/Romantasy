#include "ui/RomantasyUI.h"

#include <chrono>

#include "RE/B/BSAudioManager.h"
#include "RE/B/BSSoundHandle.h"
#include "RE/I/ID.h"

#include "input/InputMode.h"
#include "romance/RomanceManager.h"
#include "settings/Settings.h"

extern Meridian::UI::View::IViewAPI* g_MeridianView;

namespace
{
    constexpr std::string_view kLevelUpMusicPath = "Sound\\FX\\Romantasy\\levelup.wav";
    constexpr std::string_view kLevelDownMusicPath = "Sound\\FX\\Romantasy\\loselevel.wav";
    constexpr std::uint16_t kModalMusicFadeInMs = 350;
    constexpr std::uint16_t kModalMusicFadeOutMs = 650;
    constexpr std::uint32_t kModalMusicFlags = 0x1A;
    constexpr std::uint32_t kModalMusicPriority = 0x10;

    RE::Actor* ResolvePayloadActor(const nlohmann::json& data)
    {
        const auto identityText = data.value("identityFormID", std::string{});
        if (identityText.empty()) {
            return nullptr;
        }
        std::size_t parsedCharacters = 0;
        const auto identityFormID = static_cast<RE::FormID>(std::stoul(identityText, &parsedCharacters, 16));
        if (parsedCharacters != identityText.size() || identityFormID == 0) {
            return nullptr;
        }
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(identityFormID)) {
            return actor;
        }
        if (auto* baseNpc = RE::TESForm::LookupByID<RE::TESNPC>(identityFormID)) {
            return baseNpc->GetUniqueActor();
        }
        return nullptr;
    }

    bool ReadPreferenceProfile(
        const nlohmann::json& data,
        std::unordered_map<std::string, std::int32_t>& preferences)
    {
        preferences.clear();
        const auto found = data.find("preferences");
        if (found == data.end() || !found->is_object()) {
            return false;
        }
        for (auto entry = found->begin(); entry != found->end(); ++entry) {
            if (!entry.value().is_number_integer()) {
                return false;
            }
            preferences.emplace(entry.key(), entry.value().get<std::int32_t>());
        }
        return true;
    }
}

RomantasyUI& RomantasyUI::GetSingleton()
{
    static RomantasyUI singleton;
    return singleton;
}

void RomantasyUI::Initialize()
{
    if (!g_MeridianView) {
        logger::error("RomantasyUI: Meridian.View/1 not available");
        return;
    }

    Meridian::UI::View::ViewCreateInfo viewInfo{};
    viewInfo.ownerName = "romantasy";
    viewInfo.viewName = "main";
    viewInfo.startUrl = "mod://romantasy/index.html";
    viewInfo.initiallyVisible = false;
    viewInfo.onDOMReady = [](Meridian::UI::View::ViewHandle) {
        logger::info("RomantasyUI: DOM ready");
    };
    _view = g_MeridianView->CreateView(&viewInfo);

    if (_view == Meridian::UI::View::INVALID_VIEW_HANDLE) {
        logger::error("RomantasyUI: failed to create Meridian view");
        return;
    }

    RegisterListeners();
    logger::info("Romantasy UI initialized");
}

void RomantasyUI::RegisterListeners()
{
    if (!g_MeridianView) {
        return;
    }

    g_MeridianView->RegisterListener(_view, "romantasyClose", [](const char*) {
        SKSE::GetTaskInterface()->AddTask([]() {
            // Close-only: the C++ Escape key sink may have already closed the
            // dashboard for the same keypress, and during a modal-only level-up
            // popup the dashboard is not open at all. An unconditional Toggle()
            // here would reopen (or open) it instead of closing.
            auto& ui = RomantasyUI::GetSingleton();
            if (ui.IsOpen()) {
                ui.Toggle();
            }
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyPing", [](const char* arg) {
        logger::info("Romantasy UI ping: {}", arg ? arg : "");
        SKSE::GetTaskInterface()->AddTask([]() {
            RomantasyUI::GetSingleton().SendState(
                RomanceManager::GetSingleton().BuildStateJson("Bridge online")
            );
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyLevelUpDismissed", [](const char*) {
        SKSE::GetTaskInterface()->AddTask([]() {
            RomantasyUI::GetSingleton().HandleLevelUpDismissed();
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyLevelUpClosing", [](const char*) {
        SKSE::GetTaskInterface()->AddTask([]() {
            RomantasyUI::GetSingleton().HandleLevelUpClosing();
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyLevelUpModalOpened", [](const char* arg) {
        const std::string payload = arg ? arg : "";
        SKSE::GetTaskInterface()->AddTask([payload]() {
            RomantasyUI::GetSingleton().HandleLevelUpModalOpened(payload);
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyUnlockDeveloperTools", [](const char* arg) {
        const std::string confirmationCode = arg ? arg : "";
        SKSE::GetTaskInterface()->AddTask([confirmationCode]() {
            RomantasyUI::GetSingleton().HandleDeveloperToolsUnlock(confirmationCode);
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyLockDeveloperTools", [](const char*) {
        SKSE::GetTaskInterface()->AddTask([]() {
            RomantasyUI::GetSingleton().LockDeveloperTools();
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyApplyDebugStat", [](const char* arg) {
        const std::string payload = arg ? arg : "";
        SKSE::GetTaskInterface()->AddTask([payload]() {
            if (!RomantasyUI::GetSingleton().IsDeveloperToolsUnlocked()) {
                logger::warn("Romantasy debug stat request blocked; developer tools are locked");
                return;
            }

            try {
                const auto data = nlohmann::json::parse(payload.empty() ? "{}" : payload);
                if (data.is_string()) {
                    RomanceManager::GetSingleton().DebugApplyStat(data.get<std::string>(), 1);
                    return;
                }
                if (!data.is_object()) {
                    logger::warn("Romantasy debug stat payload was not an object or string");
                    return;
                }
                RomanceManager::GetSingleton().DebugApplyStat(
                    data.value("stat", std::string{}),
                    data.value("delta", 1)
                );
            } catch (const std::exception& e) {
                logger::warn("Romantasy debug stat payload failed: {}", e.what());
            }
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyApplyDebugPoints", [](const char* arg) {
        const std::string payload = arg ? arg : "";
        SKSE::GetTaskInterface()->AddTask([payload]() {
            if (!RomantasyUI::GetSingleton().IsDeveloperToolsUnlocked()) {
                logger::warn("Romantasy debug points request blocked; developer tools are locked");
                return;
            }

            try {
                const auto data = nlohmann::json::parse(payload.empty() ? "0" : payload);
                if (data.is_number_integer()) {
                    RomanceManager::GetSingleton().DebugAddPoints(data.get<std::int32_t>());
                    return;
                }
                if (!data.is_object()) {
                    logger::warn("Romantasy debug points payload was not an object or integer");
                    return;
                }
                RomanceManager::GetSingleton().DebugAddPoints(data.value("points", 0));
            } catch (const std::exception& e) {
                logger::warn("Romantasy debug points payload failed: {}", e.what());
            }
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasySetOpenMode", [](const char* arg) {
        const std::string payload = arg ? arg : "";
        SKSE::GetTaskInterface()->AddTask([payload]() {
            bool favorites = false;
            try {
                const auto data = nlohmann::json::parse(payload.empty() ? "{}" : payload);
                if (data.is_boolean()) {
                    favorites = data.get<bool>();
                } else if (data.is_object()) {
                    favorites = data.value("favorites", false);
                } else {
                    logger::warn("Romantasy set-open-mode payload was not a bool or object");
                    return;
                }
            } catch (const std::exception& e) {
                logger::warn("Romantasy set-open-mode payload failed: {}", e.what());
                return;
            }

            Settings::GetSingleton().SetOpenWithFavorites(favorites);
            Settings::GetSingleton().Save();
            InputMode::ApplyForLoadedGame();
            RomantasyUI::GetSingleton().SendState(
                RomanceManager::GetSingleton().BuildStateJson("Input mode updated."));
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasySetPreferencesManual", [](const char* arg) {
        const std::string payload = arg ? arg : "";
        SKSE::GetTaskInterface()->AddTask([payload]() {
            try {
                const auto data = nlohmann::json::parse(payload.empty() ? "{}" : payload);
                if (!data.is_object()) {
                    logger::warn("Romantasy manual-preference payload was not an object");
                    return;
                }

                const auto identityText = data.value("identityFormID", std::string{});
                if (identityText.empty()) {
                    logger::warn("Romantasy manual-preference payload did not contain an actor identity");
                    return;
                }

                std::size_t parsedCharacters = 0;
                const auto identityFormID = static_cast<RE::FormID>(std::stoul(identityText, &parsedCharacters, 16));
                if (parsedCharacters != identityText.size() || identityFormID == 0) {
                    logger::warn("Romantasy manual-preference payload contained an invalid actor identity: {}", identityText);
                    return;
                }

                auto* actor = RE::TESForm::LookupByID<RE::Actor>(identityFormID);
                if (!actor) {
                    if (auto* baseNpc = RE::TESForm::LookupByID<RE::TESNPC>(identityFormID)) {
                        actor = baseNpc->GetUniqueActor();
                    }
                }
                if (!actor) {
                    logger::warn("Romantasy manual-preference actor {:08X} was unavailable", identityFormID);
                    return;
                }

                (void)RomanceManager::GetSingleton().SetPreferencesManual(
                    actor,
                    data.value("manual", true));
            } catch (const std::exception& e) {
                logger::warn("Romantasy manual-preference payload failed: {}", e.what());
            }
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyEnrollPlayerFollower", [](const char* arg) {
        const std::string payload = arg ? arg : "";
        SKSE::GetTaskInterface()->AddTask([payload]() {
            bool enrolled = false;
            try {
                const auto data = nlohmann::json::parse(payload.empty() ? "{}" : payload);
                std::unordered_map<std::string, std::int32_t> preferences;
                auto* actor = data.is_object() ? ResolvePayloadActor(data) : nullptr;
                if (!actor || !ReadPreferenceProfile(data, preferences)) {
                    logger::warn("Romantasy enrollment payload was incomplete or invalid");
                } else {
                    enrolled = RomanceManager::GetSingleton().EnrollPlayerFollower(actor, preferences);
                }
            } catch (const std::exception& e) {
                logger::warn("Romantasy enrollment payload failed: {}", e.what());
            }
            RomantasyUI::GetSingleton().SendState(RomanceManager::GetSingleton().BuildStateJson(
                enrolled ? "A new bond was entered in the ledger." : "That companion could not be added."));
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyReplacePlayerPreferences", [](const char* arg) {
        const std::string payload = arg ? arg : "";
        SKSE::GetTaskInterface()->AddTask([payload]() {
            bool replaced = false;
            try {
                const auto data = nlohmann::json::parse(payload.empty() ? "{}" : payload);
                std::unordered_map<std::string, std::int32_t> preferences;
                auto* actor = data.is_object() ? ResolvePayloadActor(data) : nullptr;
                if (!actor || !ReadPreferenceProfile(data, preferences)) {
                    logger::warn("Romantasy player-personality payload was incomplete or invalid");
                } else {
                    replaced = RomanceManager::GetSingleton().ReplacePlayerPreferences(actor, preferences);
                }
            } catch (const std::exception& e) {
                logger::warn("Romantasy player-personality payload failed: {}", e.what());
            }
            RomantasyUI::GetSingleton().SendState(RomanceManager::GetSingleton().BuildStateJson(
                replaced ? "Companion personality sealed." : "That personality is protected."));
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyResetPlayerFollower", [](const char* arg) {
        const std::string payload = arg ? arg : "";
        SKSE::GetTaskInterface()->AddTask([payload]() {
            bool reset = false;
            try {
                const auto data = nlohmann::json::parse(payload.empty() ? "{}" : payload);
                if (auto* actor = data.is_object() ? ResolvePayloadActor(data) : nullptr) {
                    reset = RomanceManager::GetSingleton().ResetPlayerFollowerPoints(actor);
                }
            } catch (const std::exception& e) {
                logger::warn("Romantasy player-bond reset payload failed: {}", e.what());
            }
            RomantasyUI::GetSingleton().SendState(RomanceManager::GetSingleton().BuildStateJson(
                reset ? "Player-created bond reset." : "That bond cannot be reset here."));
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyRemovePlayerFollower", [](const char* arg) {
        const std::string payload = arg ? arg : "";
        SKSE::GetTaskInterface()->AddTask([payload]() {
            bool removed = false;
            try {
                const auto data = nlohmann::json::parse(payload.empty() ? "{}" : payload);
                if (auto* actor = data.is_object() ? ResolvePayloadActor(data) : nullptr) {
                    removed = RomanceManager::GetSingleton().RemovePlayerFollower(actor);
                }
            } catch (const std::exception& e) {
                logger::warn("Romantasy player-bond removal payload failed: {}", e.what());
            }
            RomantasyUI::GetSingleton().SendState(RomanceManager::GetSingleton().BuildStateJson(
                removed ? "Player-created bond removed." : "That bond is protected."));
        });
    });

    g_MeridianView->RegisterListener(_view, "romantasyUpdateSettings", [](const char* arg) {
        const std::string payload = arg ? arg : "";
        SKSE::GetTaskInterface()->AddTask([payload]() {
            try {
                const auto data = nlohmann::json::parse(payload.empty() ? "{}" : payload);
                if (!data.is_object()) {
                    logger::warn("Romantasy update-settings payload was not an object");
                    return;
                }
                auto& s = Settings::GetSingleton();
                if (data.contains("showGainModals"))    s.SetShowGainModals(data["showGainModals"].get<bool>());
                if (data.contains("showLossModals"))    s.SetShowLossModals(data["showLossModals"].get<bool>());
                if (data.contains("showAwayFollowers")) s.SetShowAwayFollowers(data["showAwayFollowers"].get<bool>());
                s.Save();
                RomantasyUI::GetSingleton().SendState(
                    RomanceManager::GetSingleton().BuildStateJson("Preferences noted."));
            } catch (const std::exception& e) {
                logger::warn("Romantasy update-settings payload failed: {}", e.what());
            }
        });
    });
}

void RomantasyUI::Toggle()
{
    if (!g_MeridianView || !g_MeridianView->IsValid(_view)) {
        logger::warn("RomantasyUI::Toggle called before UI initialization");
        return;
    }

    if (_isOpen) {
        g_MeridianView->Unfocus(_view);
        g_MeridianView->Hide(_view);
        _isOpen = false;
        _levelUpModalActive = false;
        _levelUpOnlyOpen = false;
        _levelUpQueue.clear();
        StopModalMusic();
        return;
    }

    if (_reclaimPending) {
        // A dashboard open is already queued behind the popup's focus release.
        return;
    }

    if (_levelUpOnlyOpen) {
        // The level-up popup holds Unpaused focus. Meridian applies the focus
        // mode only on the FIRST claim (FocusArbiter::TryClaim), and Unfocus()
        // releases through a queued UI task (DefaultBrowser::QueueFocusRequest)
        // rather than inline — so release now and re-enter Toggle() from the
        // same UI-task queue, after the release has landed, to claim PauseGame
        // focus fresh. The view stays visible so the popup doesn't flicker.
        g_MeridianView->Unfocus(_view);
        _levelUpOnlyOpen = false;
        _reclaimPending = true;
        SKSE::GetTaskInterface()->AddUITask([]() {
            auto& ui = RomantasyUI::GetSingleton();
            ui._reclaimPending = false;
            if (!ui._isOpen) {
                ui.Toggle();
            }
        });
        return;
    }

    g_MeridianView->Show(_view);
    const auto focusResult = g_MeridianView->TryFocus(
        _view, Meridian::UI::View::FocusMode::PauseGame);
    if (focusResult != Meridian::UI::View::FocusResult::Granted &&
        focusResult != Meridian::UI::View::FocusResult::AlreadyFocused) {
        g_MeridianView->Hide(_view);
        if (focusResult != Meridian::UI::View::FocusResult::Busy) {
            logger::warn("RomantasyUI: Meridian focus request failed ({})",
                         static_cast<std::uint32_t>(focusResult));
        }
        return;
    }

    _isOpen = true;
    _levelUpOnlyOpen = false;
    SendState(RomanceManager::GetSingleton().BuildStateJson("Romance records synchronized."));
    g_MeridianView->ExecuteJavaScript(_view, "romantasyShowPanel()");
}

bool RomantasyUI::IsOpen() const
{
    return _isOpen;
}

bool RomantasyUI::IsDeveloperToolsUnlocked() const
{
    return _developerToolsUnlocked;
}

void RomantasyUI::SendState(std::string_view message)
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    nlohmann::json data{
        { "plugin", Plugin::NAME },
        { "message", message },
        { "timestamp", seconds }
    };

    SendState(data);
}

void RomantasyUI::SendState(const nlohmann::json& state)
{
    if (!g_MeridianView || !g_MeridianView->IsValid(_view)) {
        return;
    }

    const std::string js = std::format("romantasySetState({})", state.dump());
    g_MeridianView->ExecuteJavaScript(_view, js.c_str());
}

void RomantasyUI::ShowLevelUpModal(const nlohmann::json& payload)
{
    if (!g_MeridianView || !g_MeridianView->IsValid(_view)) {
        return;
    }

    const bool isLoss = payload.value("changeDirection", std::string{ "gain" }) == "loss";
    const auto& settings = Settings::GetSingleton();
    if ((isLoss && !settings.ShowLossModals()) || (!isLoss && !settings.ShowGainModals())) {
        logger::info("Romantasy level modal suppressed by settings ({})", isLoss ? "loss" : "gain");
        return;
    }

    if (_levelUpModalActive) {
        _levelUpQueue.push_back(payload);
        logger::info("Romantasy queued relationship level modal; {} modal(s) waiting", _levelUpQueue.size());
        return;
    }

    ShowLevelUpModalNow(payload);
}

void RomantasyUI::ShowLevelUpModalNow(const nlohmann::json& payload)
{
    auto modalPayload = payload;
    const bool modalOnly = !_isOpen;
    modalPayload["modalOnly"] = modalOnly;

    if (modalOnly) {
        g_MeridianView->Show(_view);
        const auto focusResult = g_MeridianView->TryFocus(
            _view, Meridian::UI::View::FocusMode::Unpaused);
        if (focusResult != Meridian::UI::View::FocusResult::Granted &&
            focusResult != Meridian::UI::View::FocusResult::AlreadyFocused) {
            g_MeridianView->Hide(_view);
            logger::warn("RomantasyUI: level modal focus refused ({}); dropping notification",
                         static_cast<std::uint32_t>(focusResult));
            return;
        }
        _levelUpOnlyOpen = true;
    }

    _levelUpModalActive = true;
    const std::string js = std::format("romantasyShowLevelUpModal({})", modalPayload.dump());
    g_MeridianView->ExecuteJavaScript(_view, js.c_str());
}

void RomantasyUI::HandleLevelUpDismissed()
{
    _levelUpModalActive = false;
    StopModalMusic();

    if (!_levelUpQueue.empty()) {
        auto nextPayload = std::move(_levelUpQueue.front());
        _levelUpQueue.pop_front();
        ShowLevelUpModalNow(nextPayload);
        return;
    }

    if (!g_MeridianView || !g_MeridianView->IsValid(_view) || !_levelUpOnlyOpen || _isOpen) {
        _levelUpOnlyOpen = false;
        return;
    }

    g_MeridianView->Unfocus(_view);
    g_MeridianView->Hide(_view);
    _levelUpOnlyOpen = false;
}

void RomantasyUI::HandleLevelUpClosing()
{
    StopModalMusic();
}

void RomantasyUI::HandleLevelUpModalOpened(std::string_view payload)
{
    bool isLoss = false;

    try {
        const auto data = nlohmann::json::parse(payload.empty() ? "{}" : payload);
        if (data.is_object()) {
            isLoss = data.value("changeDirection", std::string{}) == "loss";
        }
    } catch (const std::exception& e) {
        logger::warn("Romantasy modal-opened payload failed: {}", e.what());
    }

    PlayModalMusic(isLoss);
}

void RomantasyUI::HandleDeveloperToolsUnlock(std::string_view confirmationCode)
{
    constexpr std::string_view developerToolsConfirmation = "ROMANTASY";

    _developerToolsUnlocked = confirmationCode == developerToolsConfirmation;
    SendDeveloperToolsUnlockResult(_developerToolsUnlocked);
}

void RomantasyUI::LockDeveloperTools()
{
    _developerToolsUnlocked = false;
}

void RomantasyUI::SendDeveloperToolsUnlockResult(bool unlocked) const
{
    if (!g_MeridianView || !g_MeridianView->IsValid(_view)) {
        return;
    }

    const nlohmann::json payload{
        { "unlocked", unlocked }
    };
    const std::string js = std::format("romantasyDeveloperToolsUnlockResult({})", payload.dump());
    g_MeridianView->ExecuteJavaScript(_view, js.c_str());
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
