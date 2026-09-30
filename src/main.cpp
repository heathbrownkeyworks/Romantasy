#include "pch.h"

#include "conditions/PointsConditionHook.h"
#include "conditions/RomanceConditionHook.h"
#include "events/SpellCastSink.h"
#include "input/InputMode.h"
#include "keyhandler/keyhandler.h"
#include "papyrus/PapyrusBridge.h"
#include "romance/RomanceManager.h"
#include "settings/Settings.h"
#include "ui/ImGuiHost.h"
#include "ui/RomantasyUI.h"

namespace
{
    void OnDataLoaded()
    {
        RomanceManager::GetSingleton().Initialize();
        PointsConditionHook::Install();
        RomanceConditionHook::Install();
        RomantasyUI::GetSingleton().Initialize();

        KeyHandler::RegisterSink();
        ImGuiHost::GetSingleton().RegisterInputSink();

        Settings::GetSingleton().Load();
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
