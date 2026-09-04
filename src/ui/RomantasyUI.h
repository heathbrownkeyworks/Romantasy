#pragma once

#include "RE/B/BSSoundHandle.h"
#include "MeridianUIAPI/ViewAPI.h"

class RomantasyUI
{
public:
    static RomantasyUI& GetSingleton();

    void Initialize();
    void Toggle();
    [[nodiscard]] bool IsOpen() const;
    [[nodiscard]] bool IsDeveloperToolsUnlocked() const;

    void SendState(std::string_view message);
    void SendState(const nlohmann::json& state);
    void ShowLevelUpModal(const nlohmann::json& payload);
    void HandleLevelUpDismissed();
    void HandleLevelUpClosing();
    void HandleLevelUpModalOpened(std::string_view payload);
    void HandleDeveloperToolsUnlock(std::string_view confirmationCode);
    void LockDeveloperTools();

private:
    RomantasyUI() = default;

    void RegisterListeners();
    void PlayModalMusic(bool isLoss);
    void SendDeveloperToolsUnlockResult(bool unlocked) const;
    void ShowLevelUpModalNow(const nlohmann::json& payload);
    void StopModalMusic(std::uint16_t fadeTimeMs = 500);

    Meridian::UI::View::ViewHandle _view = Meridian::UI::View::INVALID_VIEW_HANDLE;
    RE::BSSoundHandle _modalMusic;
    std::deque<nlohmann::json> _levelUpQueue;
    bool _isOpen = false;
    bool _levelUpOnlyOpen = false;
    bool _levelUpModalActive = false;
    bool _modalMusicActive = false;
    bool _developerToolsUnlocked = false;
    bool _reclaimPending = false;
};
