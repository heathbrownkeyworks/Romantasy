#include "pch.h"

#include "MeridianUIAPI/ViewDllLoader.h"
#include "conditions/PointsConditionHook.h"
#include "events/SpellCastSink.h"
#include "input/InputMode.h"
#include "keyhandler/keyhandler.h"
#include "papyrus/PapyrusBridge.h"
#include "romance/RomanceManager.h"
#include "settings/Settings.h"
#include "ui/RomantasyUI.h"

Meridian::UI::View::IViewAPI* g_MeridianView = nullptr;

namespace
{
    void OnInputLoaded()
    {
        if (g_MeridianView) {
            return;
        }

        Meridian::UI::Settings meridianSettings{};
        g_MeridianView = Meridian::UI::View::Query(&meridianSettings, "Romantasy");

        if (g_MeridianView) {
            logger::info("Romantasy: Meridian.View/1 acquired during kInputLoaded");
        } else {
            logger::error("Romantasy: Meridian.View/1 unavailable — browser UI disabled; core systems continue.");
        }
    }

    void OnDataLoaded()
    {
        RomanceManager::GetSingleton().Initialize();
        PointsConditionHook::Install();
        if (g_MeridianView) {
            RomantasyUI::GetSingleton().Initialize();
        } else {
            logger::warn("Romantasy: skipping browser UI initialization (no Meridian)");
        }

        KeyHandler::RegisterSink();

        Settings::GetSingleton().Load();
        SpellCastSink::RegisterSink();
        InputMode::RefreshHotkey();  // registers Ctrl+R unless favorites mode is on

        logger::info("{} systems initialized", Plugin::NAME);
    }

    void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
    {
        switch (message->type) {
        case SKSE::MessagingInterface::kInputLoaded:
            OnInputLoaded();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            OnDataLoaded();
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            InputMode::ApplyForLoadedGame();
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
