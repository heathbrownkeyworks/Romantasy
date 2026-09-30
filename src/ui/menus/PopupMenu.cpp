#include "ui/menus/PopupMenu.h"

#include "RE/B/BSUIMessageData.h"
#include "RE/U/UI.h"
#include "RE/U/UserEvents.h"

#include "ui/RomantasyUI.h"

PopupMenu::PopupMenu()
{
    using Flag = RE::UI_MENU_FLAGS;
    menuFlags.set(Flag::kUsesMenuContext, Flag::kCustomRendering, Flag::kAllowSaving);
    depthPriority = 11;
    inputContext.set(RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
}

bool PopupMenu::Register()
{
    if (auto* ui = RE::UI::GetSingleton()) {
        ui->Register(MENU_NAME, Create);
        logger::info("RomantasyUI: registered {}", MENU_NAME);
        return true;
    }
    logger::error("RomantasyUI: UI singleton unavailable; {} not registered", MENU_NAME);
    return false;
}

RE::IMenu* PopupMenu::Create()
{
    return new PopupMenu();
}

void PopupMenu::PostDisplay()
{
    RomantasyUI::GetSingleton().DrawPopupFrame();
}

RE::UI_MESSAGE_RESULTS PopupMenu::ProcessMessage(RE::UIMessage& a_message)
{
    switch (*a_message.type) {
    case RE::UI_MESSAGE_TYPE::kShow:
        RomantasyUI::GetSingleton().OnPopupShown();
        break;
    case RE::UI_MESSAGE_TYPE::kHide:
    case RE::UI_MESSAGE_TYPE::kForceHide:
        RomantasyUI::GetSingleton().OnPopupHidden();
        break;
    case RE::UI_MESSAGE_TYPE::kScaleformEvent:
        return RE::UI_MESSAGE_RESULTS::kHandled;
    case RE::UI_MESSAGE_TYPE::kUserEvent: {
        const auto* data = static_cast<const RE::BSUIMessageData*>(a_message.data);
        const auto* events = RE::UserEvents::GetSingleton();
        if (data && events && (data->fixedStr == events->cancel || data->fixedStr == events->accept || data->fixedStr == events->activate)) {
            RomantasyUI::GetSingleton().RequestPopupDismiss();
        }
        return RE::UI_MESSAGE_RESULTS::kHandled;
    }
    default:
        break;
    }
    return RE::IMenu::ProcessMessage(a_message);
}
