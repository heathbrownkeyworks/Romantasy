#pragma once

// Paused Bond Ledger console. Skyrim owns pause, cursor and input context;
// PostDisplay runs the ImGui frame through RomantasyUI.
class LedgerMenu final : public RE::IMenu
{
public:
    static constexpr std::string_view MENU_NAME = "RomantasyLedgerMenu";

    static bool Register();
    static RE::IMenu* Create();

    void PostDisplay() override;
    RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override;

private:
    LedgerMenu();
};
