#pragma once

#include "ui/screens/Widgets.h"

#include <atomic>
#include <span>

namespace romantasy::ui
{
    // Skyrim's input sink owns the native device state, including disconnects.
    // The standalone desktop preview instead calls UpdateFromBackend each frame.
    class ControllerInput
    {
    public:
        void SetGamepad(bool gamepad) noexcept { _gamepad.store(gamepad, std::memory_order_relaxed); }
        void UpdateFromBackend(bool trackMousePosition = true);
        [[nodiscard]] bool UsingGamepad() const;

    private:
        std::atomic<bool> _gamepad{ false };
    };

    enum class ControllerHintScope { Console, Enrollment, Modal, TextField, ModalTextField, Popup };
    struct ControllerHint { const char* key; const char* action; };
    inline constexpr float ControllerHintsHeight = 56.0f;

    [[nodiscard]] std::span<const ControllerHint> ControllerHintsFor(ControllerHintScope scope);
    void DrawControllerHints(const widgets::Ctx& c, ControllerHintScope scope);
}
