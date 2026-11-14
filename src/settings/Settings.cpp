#include "settings/Settings.h"
#include "ui/PageRequests.h"
#include "util/AtomicFile.h"

#include <filesystem>
#include <fstream>

namespace
{
    constexpr std::string_view kSettingsDir  = "Data/SKSE/Plugins/Romantasy";
    constexpr std::string_view kSettingsPath = "Data/SKSE/Plugins/Romantasy/settings.json";

    // A string key parsed by `parse`; a missing key, a non-string or an unknown string leaves `out` as it was.
    template <class T, class Parse>
    void ReadKey(const nlohmann::json& j, const char* name, T& out, Parse parse)
    {
        const auto it = j.find(name);
        if (it == j.end() || !it->is_string()) {
            return;
        }
        if (const auto parsed = parse(it->template get_ref<const std::string&>())) {
            out = *parsed;
        }
    }

    // A boolean key; a missing key keeps the default, a wrong type keeps it too and is named once in the log.
    void ReadSwitch(const nlohmann::json& j, const char* name, std::atomic_bool& out)
    {
        const auto it = j.find(name);
        if (it == j.end()) {
            return;
        }
        if (it->is_boolean()) {
            out = it->get<bool>();
        } else {
            logger::warn("Romantasy settings: \"{}\" must be true or false; found {} {}. It keeps its default.",
                name, it->type_name(), it->dump(-1, ' ', true).substr(0, 40));
        }
    }
}

Settings& Settings::GetSingleton()
{
    static Settings singleton;
    return singleton;
}

void Settings::SetRosterOrder(std::string_view value)
{
    if (const auto key = romantasy::ui::ParseRosterOrder(value)) {
        _rosterOrder = *key;
    }
}

void Settings::SetLedgerSize(std::string_view value)
{
    if (const auto key = romantasy::ui::ParseLedgerSize(value)) {
        _ledgerSize = *key;
    }
}

void Settings::ApplyLogLevel() const
{
    const auto level = _detailedLog ? spdlog::level::debug : spdlog::level::info;
    if (const auto log = spdlog::default_logger()) {
        log->set_level(level);
        log->flush_on(level);
    }
}

void Settings::Load()
{
    _loadFailed = false;
    try {
        std::ifstream file{ std::filesystem::path(kSettingsPath) };
        if (!file.is_open()) {
            logger::info("Romantasy settings.json not found; using defaults (hotkey mode)");
            ApplyLogLevel();
            return;
        }
        nlohmann::json j;
        file >> j;
        _openWithFavorites = j.value("openWithFavorites", _openWithFavorites);
        _showGainModals    = j.value("showGainModals", _showGainModals);
        _showLossModals    = j.value("showLossModals", _showLossModals);
        _showAwayFollowers = j.value("showAwayFollowers", _showAwayFollowers);
        _detailedLog       = j.value("detailedLog", _detailedLog.load());
        ReadKey(j, "pace", _pace, romantasy::pacing::ParsePace);
        ReadKey(j, "tierMoment", _tierMoment, romantasy::pacing::ParseTierMoment);
        ReadKey(j, "whisperCorner", _whisperCorner, romantasy::ui::whispers::ParseWhisperCorner);
        // A missing or non-boolean value keeps the default (on).
        if (const auto whispers = j.find("whispers"); whispers != j.end()) {
            if (whispers->is_boolean()) {
                _whispers = whispers->get<bool>();
            }
        }
        ReadKey(j, "rosterOrder", _rosterOrder, romantasy::ui::ParseRosterOrder);
        ReadKey(j, "ledgerSize", _ledgerSize, romantasy::ui::ParseLedgerSize);
        ReadSwitch(j, "highContrast", _highContrast);
        ReadSwitch(j, "reduceMotion", _reduceMotion);
        ReadSwitch(j, "skyrimNetBridge", _skyrimNetBridge);
        ReadSwitch(j, "skyrimNetConversation", _skyrimNetConversation);
        ReadSwitch(j, "ostimMoments", _ostimMoments);
        ReadSwitch(j, "ostimHold", _ostimHold);
        // A missing or non-boolean value keeps the default; a non-boolean one is named in the log.
        if (const auto author = j.find("authorMode"); author != j.end()) {
            if (author->is_boolean()) {
                _authorMode = author->get<bool>();
            } else {
                logger::warn("Romantasy settings: \"authorMode\" must be true or false; found {} {}. Author mode stays off.",
                    author->type_name(), author->dump(-1, ' ', true).substr(0, 40));
            }
        }
        logger::info("Romantasy settings loaded: openWithFavorites={}, showGainModals={}, showLossModals={}, showAwayFollowers={}, detailedLog={}, pace={}, tierMoment={}, whispers={}, whisperCorner={}, rosterOrder={}, ledgerSize={}, highContrast={}, reduceMotion={}, skyrimNetBridge={}, skyrimNetConversation={}, ostimMoments={}, ostimHold={}, authorMode={}",
            _openWithFavorites, _showGainModals, _showLossModals, _showAwayFollowers, _detailedLog.load(),
            romantasy::pacing::Key(_pace), romantasy::pacing::Key(_tierMoment), _whispers.load(),
            romantasy::ui::whispers::Key(_whisperCorner.load()), _rosterOrder.load(), _ledgerSize.load(),
            _highContrast.load(), _reduceMotion.load(), _skyrimNetBridge.load(), _skyrimNetConversation.load(),
            _ostimMoments.load(), _ostimHold.load(), _authorMode.load());
    } catch (const std::exception& e) {
        _loadFailed = true;
        logger::warn("Romantasy settings load failed ({}); using defaults", e.what());
    }
    ApplyLogLevel();
}

void Settings::Save() const
{
    _saveFailed = true;
    try {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(kSettingsDir), ec);
        nlohmann::json j;
        j["openWithFavorites"] = _openWithFavorites;
        j["showGainModals"]    = _showGainModals;
        j["showLossModals"]    = _showLossModals;
        j["showAwayFollowers"] = _showAwayFollowers;
        j["detailedLog"]       = _detailedLog.load();
        j["pace"]              = std::string(romantasy::pacing::Key(_pace));
        j["tierMoment"]        = std::string(romantasy::pacing::Key(_tierMoment));
        j["whispers"]          = _whispers.load();
        j["whisperCorner"]     = std::string(romantasy::ui::whispers::Key(_whisperCorner.load()));
        j["rosterOrder"]       = std::string(_rosterOrder.load());
        j["ledgerSize"]        = std::string(_ledgerSize.load());
        j["highContrast"]      = _highContrast.load();
        j["reduceMotion"]      = _reduceMotion.load();
        j["skyrimNetBridge"]   = _skyrimNetBridge.load();
        j["skyrimNetConversation"] = _skyrimNetConversation.load();
        j["ostimMoments"]      = _ostimMoments.load();
        j["ostimHold"]         = _ostimHold.load();
        j["authorMode"]        = _authorMode.load();
        if (!romantasy::files::WriteTextAtomically(std::filesystem::path(kSettingsPath), j.dump(2))) {
            logger::warn("Romantasy settings save failed; could not write {}", kSettingsPath);
            return;
        }
        _saveFailed = false;
        logger::info("Romantasy settings saved: openWithFavorites={}", _openWithFavorites);
    } catch (const std::exception& e) {
        logger::warn("Romantasy settings save failed ({})", e.what());
    }
}
