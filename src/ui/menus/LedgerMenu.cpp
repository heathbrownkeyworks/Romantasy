#include "ui/menus/LedgerMenu.h"

#include "RE/B/BSUIMessageData.h"
#include "RE/U/UI.h"
#include "RE/U/UserEvents.h"

#include "ui/RomantasyUI.h"

LedgerMenu::LedgerMenu()
{
    using Flag = RE::UI_MENU_FLAGS;
    menuFlags.set(Flag::kPausesGame, Flag::kUsesCursor, Flag::kUpdateUsesCursor,
        Flag::kUsesMenuContext, Flag::kCustomRendering, Flag::kDisablePauseMenu);
    depthPriority = 11;
    inputContext.set(RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
}

bool LedgerMenu::Register()
{
    if (auto* ui = RE::UI::GetSingleton()) {
        ui->Register(MENU_NAME, Create);
        logger::info("RomantasyUI: registered {}", MENU_NAME);
        return true;
    }
    logger::error("RomantasyUI: UI singleton unavailable; {} not registered", MENU_NAME);
    return false;
}

RE::IMenu* LedgerMenu::Create()
{
    return new LedgerMenu();
}

void LedgerMenu::PostDisplay()
{
    RomantasyUI::GetSingleton().DrawLedgerFrame();
}

RE::UI_MESSAGE_RESULTS LedgerMenu::ProcessMessage(RE::UIMessage& a_message)
{
    switch (*a_message.type) {
    case RE::UI_MESSAGE_TYPE::kShow:
        RomantasyUI::GetSingleton().OnLedgerShown();
        break;
    case RE::UI_MESSAGE_TYPE::kHide:
    case RE::UI_MESSAGE_TYPE::kForceHide:
        RomantasyUI::GetSingleton().OnLedgerHidden();
        break;
    case RE::UI_MESSAGE_TYPE::kScaleformEvent:
        return RE::UI_MESSAGE_RESULTS::kHandled;  // no movie to forward to
    case RE::UI_MESSAGE_TYPE::kUserEvent: {
        const auto* data = static_cast<const RE::BSUIMessageData*>(a_message.data);
        const auto* events = RE::UserEvents::GetSingleton();
        if (data && events && (data->fixedStr == events->cancel || data->fixedStr == events->back)) {
            RomantasyUI::GetSingleton().RequestClose();
        }
        return RE::UI_MESSAGE_RESULTS::kHandled;
    }
    default:
        break;
    }
    return RE::IMenu::ProcessMessage(a_message);
}
