#include "check.h"

#include "ui/screens/InputMap.h"

#include <cstdint>

using romantasy::ui::input::IsTextCharacter;
using romantasy::ui::input::ScanCodeToImGuiKey;

TEST(input_map_translates_the_keys_the_screens_use)
{
    struct Case
    {
        std::uint32_t scanCode;
        ImGuiKey expected;
    };

    // Modifiers (0x1D Ctrl, 0x2A Shift, 0x38 Alt) stay unmapped: UpdateModifiers()
    // polls them per frame. Letters and digits are mapped so the input widget
    // gets Ctrl+A / Ctrl+V and word navigation; characters arrive separately.
    constexpr Case cases[] = {
        { 0x01, ImGuiKey_Escape },
        { 0x1C, ImGuiKey_Enter },
        { 0x9C, ImGuiKey_KeypadEnter },
        { 0x39, ImGuiKey_Space },
        { 0x0F, ImGuiKey_Tab },
        { 0xC8, ImGuiKey_UpArrow },
        { 0xD0, ImGuiKey_DownArrow },
        { 0xCB, ImGuiKey_LeftArrow },
        { 0xCD, ImGuiKey_RightArrow },
        { 0xC9, ImGuiKey_PageUp },
        { 0xD1, ImGuiKey_PageDown },
        { 0xC7, ImGuiKey_Home },
        { 0xCF, ImGuiKey_End },
        { 0x0E, ImGuiKey_Backspace },
        { 0xD3, ImGuiKey_Delete },
        { 0xD2, ImGuiKey_Insert },
        { 0x1E, ImGuiKey_A },
        { 0x13, ImGuiKey_R },
        { 0x2F, ImGuiKey_V },
        { 0x2C, ImGuiKey_Z },
        { 0x0B, ImGuiKey_0 },
        { 0x02, ImGuiKey_1 },
        { 0x0A, ImGuiKey_9 },
        { 0x1D, ImGuiKey_None },
        { 0x2A, ImGuiKey_None },
        { 0x38, ImGuiKey_None },
        { 0x3B, ImGuiKey_None },  // F1
    };

    for (const auto& testCase : cases) {
        CHECK(ScanCodeToImGuiKey(testCase.scanCode) == testCase.expected);
    }
}

TEST(input_map_text_characters_exclude_control_codes)
{
    CHECK(!IsTextCharacter(0x00));
    CHECK(!IsTextCharacter(0x08));  // backspace arrives as a key, not a character
    CHECK(!IsTextCharacter(0x0D));  // enter
    CHECK(!IsTextCharacter(0x12));  // the control code Ctrl+R produces
    CHECK(!IsTextCharacter(0x7F));  // DEL
    CHECK(IsTextCharacter(' '));
    CHECK(IsTextCharacter('a'));
    CHECK(IsTextCharacter('6'));
    CHECK(IsTextCharacter(0xE9));   // e-acute, Latin-1
}
