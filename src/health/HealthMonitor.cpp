#include "health/HealthMonitor.h"

#include "conditions/PointsConditionHook.h"
#include "conditions/RomanceConditionHook.h"
#include "input/InputMode.h"
#include "ostim/OStimBridge.h"
#include "romance/RomanceManager.h"
#include "settings/Settings.h"
#include "skyrimnet/SkyrimNetBridge.h"
#include "ui/magelight/MagelightHost.h"
#include "ui/magelight/WhisperView.h"

namespace romantasy::health
{
    Facts Gather()
    {
        Facts facts;
        facts.version = std::format("{}.{}.{}", Plugin::VERSION.major(), Plugin::VERSION.minor(), Plugin::VERSION.patch());

        const auto& host = romantasy::ui::MagelightHost::GetSingleton();
        facts.host = host.HostStatus();
        facts.hostVersion = host.HostVersion();

        RomanceManager::GetSingleton().FillHealth(facts);
        facts.romancePointsFaction = PointsConditionHook::FactionFound();
        facts.power = InputMode::PowerFound();
        facts.graphHook = RomanceConditionHook::Installed();
        // Without ROM_RomancePoints the hook is never tried; the missing faction is the plugin item's problem.
        facts.factionRankHook = PointsConditionHook::Installed() || !PointsConditionHook::FactionFound();

        const auto& settings = Settings::GetSingleton();
        facts.settingsUnreadable = settings.LoadFailed();
        facts.settingsUnsaved = settings.SaveFailed();
        facts.authorMode = settings.AuthorMode();
        facts.whispers = settings.Whispers();
        facts.whisperView = romantasy::ui::WhisperView::GetSingleton().Available();
        const auto& skyrimNet = SkyrimNetBridge::GetSingleton();
        facts.skyrimNet = skyrimNet.Found() ? SkyrimNetState::Found : skyrimNet.TooOld() ? SkyrimNetState::TooOld : SkyrimNetState::Absent;
        facts.skyrimNetBridge = settings.SkyrimNetBridge();
        facts.skyrimNetBondLineMissing = skyrimNet.DecoratorFailed();
        const auto& ostim = OStimBridge::GetSingleton();
        facts.ostim = ostim.Found() ? OStimState::Found : ostim.TooOld() ? OStimState::TooOld : OStimState::Absent;
        facts.ostimMoments = settings.OStimMoments();
        return facts;
    }

    void AnnounceOnce()
    {
        const auto facts = Gather();
        const auto report = BuildReport(facts);

        if (report.items.empty()) {
            logger::info("Romantasy health: ok");
        }
        for (const auto& item : report.items) {
            std::string detail;  // one log line: the detail's lines joined by "; "
            for (const char c : item.detail) {
                if (c == '\n') detail += "; ";
                else detail += c;
            }
            const auto line = std::format("Romantasy health ({}): {}. {}", item.id, item.title, detail);
            switch (item.level) {
            case Level::Error: logger::error("{}", line); break;
            case Level::Warn:  logger::warn("{}", line); break;
            default:           logger::info("{}", line); break;
            }
        }

        static bool announced = false;
        if (announced) return;
        announced = true;
        if (auto line = HudLine(report, facts)) {
            SKSE::GetTaskInterface()->AddTask([line = std::move(*line)]() {
                RE::SendHUDMessage::ShowHUDMessage(line.c_str());
            });
        }
    }
}
