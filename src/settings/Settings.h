#pragma once

// Global Romantasy settings persisted to Data/SKSE/Plugins/Romantasy/settings.json.
// Persists input mode (Ctrl+R hotkey vs Favorites-menu power) and dashboard toggles.
class Settings
{
public:
    static Settings& GetSingleton();

    void Load();
    void Save() const;

    [[nodiscard]] bool OpenWithFavorites() const { return _openWithFavorites; }
    void SetOpenWithFavorites(bool value) { _openWithFavorites = value; }

    [[nodiscard]] bool ShowGainModals() const { return _showGainModals; }
    void SetShowGainModals(bool value) { _showGainModals = value; }

    [[nodiscard]] bool ShowLossModals() const { return _showLossModals; }
    void SetShowLossModals(bool value) { _showLossModals = value; }

    [[nodiscard]] bool ShowAwayFollowers() const { return _showAwayFollowers; }
    void SetShowAwayFollowers(bool value) { _showAwayFollowers = value; }

private:
    Settings() = default;

    bool _openWithFavorites = false;  // default: Ctrl+R hotkey
    bool _showGainModals = true;
    bool _showLossModals = true;
    bool _showAwayFollowers = true;
};
