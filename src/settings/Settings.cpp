#include "settings/Settings.h"

#include <filesystem>
#include <fstream>

namespace
{
    constexpr std::string_view kSettingsDir  = "Data/SKSE/Plugins/Romantasy";
    constexpr std::string_view kSettingsPath = "Data/SKSE/Plugins/Romantasy/settings.json";
}

Settings& Settings::GetSingleton()
{
    static Settings singleton;
    return singleton;
}

void Settings::Load()
{
    try {
        std::ifstream file{ std::filesystem::path(kSettingsPath) };
        if (!file.is_open()) {
            logger::info("Romantasy settings.json not found; using defaults (hotkey mode)");
            return;
        }
        nlohmann::json j;
        file >> j;
        _openWithFavorites = j.value("openWithFavorites", _openWithFavorites);
        _showGainModals    = j.value("showGainModals", _showGainModals);
        _showLossModals    = j.value("showLossModals", _showLossModals);
        _showAwayFollowers = j.value("showAwayFollowers", _showAwayFollowers);
        logger::info("Romantasy settings loaded: openWithFavorites={}", _openWithFavorites);
    } catch (const std::exception& e) {
        logger::warn("Romantasy settings load failed ({}); using defaults", e.what());
    }
}

void Settings::Save() const
{
    try {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(kSettingsDir), ec);
        nlohmann::json j;
        j["openWithFavorites"] = _openWithFavorites;
        j["showGainModals"]    = _showGainModals;
        j["showLossModals"]    = _showLossModals;
        j["showAwayFollowers"] = _showAwayFollowers;
        std::ofstream file{ std::filesystem::path(kSettingsPath) };
        if (!file.is_open()) {
            logger::warn("Romantasy settings save failed; could not open {}", kSettingsPath);
            return;
        }
        file << j.dump(2);
        logger::info("Romantasy settings saved: openWithFavorites={}", _openWithFavorites);
    } catch (const std::exception& e) {
        logger::warn("Romantasy settings save failed ({})", e.what());
    }
}
