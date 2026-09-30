#pragma once

#include <imgui.h>

#include <cstdint>

namespace romantasy::ui::input
{
    // DirectInput scan code -> ImGui key. Modifiers are deliberately absent:
    // ImGuiHost::UpdateModifiers() polls them once per frame and is the single
    // source for Ctrl/Shift/Alt. Letters and digits are mapped so the input
    // widget gets Ctrl+A / Ctrl+C / Ctrl+V / Ctrl+X and word navigation; the
    // characters themselves arrive through RE::CharEvent.
    [[nodiscard]] constexpr ImGuiKey ScanCodeToImGuiKey(std::uint32_t scanCode) noexcept
    {
        switch (scanCode) {
        case 0x01: return ImGuiKey_Escape;
        case 0x0F: return ImGuiKey_Tab;
        case 0x1C: return ImGuiKey_Enter;
        case 0x9C: return ImGuiKey_KeypadEnter;
        case 0x39: return ImGuiKey_Space;
        case 0x0E: return ImGuiKey_Backspace;
        case 0xD3: return ImGuiKey_Delete;
        case 0xD2: return ImGuiKey_Insert;
        case 0xC8: return ImGuiKey_UpArrow;
        case 0xD0: return ImGuiKey_DownArrow;
        case 0xCB: return ImGuiKey_LeftArrow;
        case 0xCD: return ImGuiKey_RightArrow;
        case 0xC9: return ImGuiKey_PageUp;
        case 0xD1: return ImGuiKey_PageDown;
        case 0xC7: return ImGuiKey_Home;
        case 0xCF: return ImGuiKey_End;
        case 0x1E: return ImGuiKey_A;
        case 0x30: return ImGuiKey_B;
        case 0x2E: return ImGuiKey_C;
        case 0x20: return ImGuiKey_D;
        case 0x12: return ImGuiKey_E;
        case 0x21: return ImGuiKey_F;
        case 0x22: return ImGuiKey_G;
        case 0x23: return ImGuiKey_H;
        case 0x17: return ImGuiKey_I;
        case 0x24: return ImGuiKey_J;
        case 0x25: return ImGuiKey_K;
        case 0x26: return ImGuiKey_L;
        case 0x32: return ImGuiKey_M;
        case 0x31: return ImGuiKey_N;
        case 0x18: return ImGuiKey_O;
        case 0x19: return ImGuiKey_P;
        case 0x10: return ImGuiKey_Q;
        case 0x13: return ImGuiKey_R;
        case 0x1F: return ImGuiKey_S;
        case 0x14: return ImGuiKey_T;
        case 0x16: return ImGuiKey_U;
        case 0x2F: return ImGuiKey_V;
        case 0x11: return ImGuiKey_W;
        case 0x2D: return ImGuiKey_X;
        case 0x15: return ImGuiKey_Y;
        case 0x2C: return ImGuiKey_Z;
        case 0x0B: return ImGuiKey_0;
        case 0x02: return ImGuiKey_1;
        case 0x03: return ImGuiKey_2;
        case 0x04: return ImGuiKey_3;
        case 0x05: return ImGuiKey_4;
        case 0x06: return ImGuiKey_5;
        case 0x07: return ImGuiKey_6;
        case 0x08: return ImGuiKey_7;
        case 0x09: return ImGuiKey_8;
        case 0x0A: return ImGuiKey_9;
        default: return ImGuiKey_None;
        }
    }

    // Characters worth typing into a field: everything printable, no control
    // codes (Backspace, Enter and Ctrl+letter arrive as keys, not characters).
    [[nodiscard]] constexpr bool IsTextCharacter(std::uint32_t code) noexcept
    {
        return code >= 0x20 && code != 0x7F;
    }
}
