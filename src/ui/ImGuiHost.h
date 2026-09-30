#pragma once

#include "ui/screens/Theme.h"
#include "ui/screens/ControllerHints.h"

#include <atomic>
#include <mutex>
#include <vector>

struct ImGuiContext;

// Owns the single ImGui context for Romantasy. Initializes lazily from the
// game's renderer the first time a Romantasy menu draws, feeds ImGui from an
// SKSE input sink while a Romantasy menu is active, and runs one ImGui frame
// per BeginFrame/EndFrame pair (called from IMenu::PostDisplay).
class ImGuiHost : public RE::BSTEventSink<RE::InputEvent*>
{
public:
    static ImGuiHost& GetSingleton();

    // Safe to call every frame. Returns false (logging once) when the renderer
    // singleton, device or swapchain is unavailable.
    bool EnsureInitialized();
    [[nodiscard]] bool IsReady() const noexcept { return _ready.load(std::memory_order_acquire); }

    // Registers the input sink with BSInputDeviceManager. Called once at
    // kDataLoaded so input is wired up before the first menu opens.
    void RegisterInputSink();

    // While active the input sink forwards keys, mouse buttons and wheel to ImGui.
    void SetActive(bool active) noexcept { _active.store(active, std::memory_order_release); }
    [[nodiscard]] bool IsActive() const noexcept { return _active.load(std::memory_order_acquire); }

    // True when the last frame ended with an ImGui text field active. Read by
    // the Ctrl+R hotkey so typing an R never toggles the console.
    [[nodiscard]] bool WantsTextInput() const noexcept { return _wantsTextInput.load(std::memory_order_acquire); }

    // One ImGui frame. BeginFrame returns false when not ready; EndFrame is a
    // no-op unless BeginFrame succeeded.
    bool BeginFrame();
    void EndFrame();
    void ClearInput();

    [[nodiscard]] const romantasy::ui::theme::Fonts& Fonts() const noexcept { return _fonts; }
    [[nodiscard]] romantasy::ui::theme::Canvas Canvas() const;
    [[nodiscard]] float DeltaTime() const;
    [[nodiscard]] bool UsingGamepad() const { return _controllerInput.UsingGamepad(); }

    RE::BSEventNotifyControl ProcessEvent(
        RE::InputEvent* const* a_eventList,
        RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

private:
    ImGuiHost() = default;
    ~ImGuiHost() override = default;

    bool Initialize();
    void UpdateMousePosition();
    void UpdateModifiers();

    struct PendingInput
    {
        enum class Kind : std::uint8_t { Key, MouseButton, MouseWheel, Char };
        Kind kind = Kind::Key;
        int code = 0;        // ImGuiKey for Key, button index for MouseButton, codepoint for Char
        bool down = false;
        float wheel = 0.0f;  // MouseWheel only
    };

    // Filled by the input sink (any thread), drained by BeginFrame (frame thread).
    void DrainPendingInput(ImGuiIO& io);
    std::mutex _inputLock;
    std::vector<PendingInput> _pendingInput;

    ImGuiContext* _context = nullptr;
    romantasy::ui::theme::Fonts _fonts;
    romantasy::ui::ControllerInput _controllerInput;
    std::atomic<bool> _active{ false };
    std::atomic<bool> _ready{ false };
    std::atomic<bool> _wantsTextInput{ false };
    bool _failed = false;
    bool _inFrame = false;
    bool _sinkRegistered = false;
    bool _firstFrameLogged = false;
    bool _firstCharLogged = false;
};
