#include "pch.h"

#include "conditions/PointsConditionHook.h"
#include "conditions/RomanceConditionHook.h"
#include "events/SpellCastSink.h"
#include "health/HealthMonitor.h"
#include "input/InputMode.h"
#include "keyhandler/keyhandler.h"
#include "ostim/OStimBridge.h"
#include "papyrus/PapyrusBridge.h"
#include "romance/RomanceManager.h"
#include "settings/Settings.h"
#include "skyrimnet/SkyrimNetBridge.h"
#include "ui/RomantasyUI.h"

namespace
{
    void OnDataLoaded()
    {
        // First, so the log level (Detailed log) applies to the startup lines below.
        Settings::GetSingleton().Load();
        // Before anything can raise an event: the bridge finds SkyrimNet (or logs that it is absent) once.
        SkyrimNetBridge::GetSingleton().Start();
        // OStim, the same way: found through its own interface or logged as absent, once.
        OStimBridge::GetSingleton().Start();

        RomanceManager::GetSingleton().Initialize();
        PointsConditionHook::Install();
        RomanceConditionHook::Install();
        RomantasyUI::GetSingleton().Initialize();

        KeyHandler::RegisterSink();

        SpellCastSink::RegisterSink();
        InputMode::RefreshHotkey();  // Ctrl+R unless favorites mode is on

        logger::info("{} systems initialized", Plugin::NAME);
    }

    void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
    {
        switch (message->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            OnDataLoaded();
            break;
        case SKSE::MessagingInterface::kPreLoadGame:
            RomantasyUI::GetSingleton().Close();
            OStimBridge::GetSingleton().OnGameReset();  // OStim drops its threads on a load without a stop event
            break;
        case SKSE::MessagingInterface::kNewGame:
            RomantasyUI::GetSingleton().Close();
            OStimBridge::GetSingleton().OnGameReset();
            InputMode::ApplyForLoadedGame();
            SkyrimNetBridge::GetSingleton().RegisterDecorator();  // SkyrimNet's Papyrus registrations do not outlive a load
            romantasy::health::AnnounceOnce();
            RomanceManager::GetSingleton().OnGameLoaded();
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            InputMode::ApplyForLoadedGame();
            SkyrimNetBridge::GetSingleton().RegisterDecorator();
            romantasy::health::AnnounceOnce();
            // data is the load's success flag
            if (message->data) {
                RomanceManager::GetSingleton().OnGameLoaded();
            }
            break;
        default:
            break;
        }
    }
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
    REL::Module::reset();

    auto* messaging = reinterpret_cast<SKSE::MessagingInterface*>(
        a_skse->QueryInterface(SKSE::LoadInterface::kMessaging)
    );

    if (!messaging) {
        logger::critical("Failed to load messaging interface");
        return false;
    }

    logger::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());

    SKSE::Init(a_skse);
    RomanceManager::GetSingleton().RegisterSerializationCallbacks(SKSE::GetSerializationInterface());
    if (const auto* papyrus = SKSE::GetPapyrusInterface()) {
        papyrus->Register(PapyrusBridge::Register);
    } else {
        logger::critical("Failed to load Papyrus interface");
        return false;
    }
    messaging->RegisterListener("SKSE", SKSEMessageHandler);

    return true;
}
