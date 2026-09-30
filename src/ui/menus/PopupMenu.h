#pragma once

// Unpaused level-change popup. Holds the menu input context so A/B/Enter/Esc
// reach it while the world keeps running. Draws only while the ledger is closed.
class PopupMenu final : public RE::IMenu
{
public:
    static constexpr std::string_view MENU_NAME = "RomantasyPopupMenu";

    static bool Register();
    static RE::IMenu* Create();

    void PostDisplay() override;
    RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override;

private:
    PopupMenu();
};
