#include "ui/ImGuiHost.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#include "ui/screens/InputMap.h"

#include <algorithm>

#include "RE/B/BSInputDeviceManager.h"
#include "RE/C/CharEvent.h"
#include "RE/C/CursorMenu.h"
#include "RE/D/DeviceConnectEvent.h"
#include "RE/M/MenuCursor.h"
#include "RE/M/MouseMoveEvent.h"
#include "RE/R/Renderer.h"
#include "RE/T/ThumbstickEvent.h"
#include "RE/U/UI.h"

namespace
{
    constexpr const char* kFontDir = "Data/SKSE/Plugins/Romantasy/fonts";

    // BSWin32MouseDevice button ids.
    constexpr std::uint32_t kMouseLeft = 0;
    constexpr std::uint32_t kMouseRight = 1;
    constexpr std::uint32_t kMouseMiddle = 2;
    constexpr std::uint32_t kMouseWheelUp = 8;
    constexpr std::uint32_t kMouseWheelDown = 9;
}

ImGuiHost& ImGuiHost::GetSingleton()
{
    static ImGuiHost singleton;
    return singleton;
}

bool ImGuiHost::EnsureInitialized()
{
    if (_ready) {
        return true;
    }
    if (_failed) {
        return false;
    }
    if (!Initialize()) {
        _failed = true;
        logger::error("RomantasyUI: ImGui initialization failed; the dashboard stays disabled (core systems continue)");
        return false;
    }
    return true;
}

bool ImGuiHost::Initialize()
{
    auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
    if (!renderer) {
        logger::error("RomantasyUI: renderer singleton unavailable");
        return false;
    }
    auto& data = renderer->GetRuntimeData();
    auto* device = reinterpret_cast<ID3D11Device*>(data.forwarder);
    auto* context = reinterpret_cast<ID3D11DeviceContext*>(data.context);
    auto* swapChain = reinterpret_cast<IDXGISwapChain*>(data.renderWindows[0].swapChain);
    if (!device || !context || !swapChain) {
        logger::error("RomantasyUI: renderer device/context/swapchain not ready");
        return false;
    }
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapChain->GetDesc(&desc)) || !desc.OutputWindow) {
        logger::error("RomantasyUI: swapchain has no output window");
        return false;
    }

    IMGUI_CHECKVERSION();
    _context = ImGui::CreateContext();
    ImGui::SetCurrentContext(_context);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NoMouseCursorChange;
    io.MouseDrawCursor = false;

    if (!romantasy::ui::theme::LoadFonts(io, kFontDir, _fonts)) {
        logger::warn("RomantasyUI: one or more fonts missing in {}; falling back to ImGui's default font", kFontDir);
    }

    if (!ImGui_ImplWin32_Init(desc.OutputWindow)) {
        logger::error("RomantasyUI: ImGui Win32 backend init failed");
        _fonts = {};
        ImGui::DestroyContext(_context);
        _context = nullptr;
        return false;
    }
    if (!ImGui_ImplDX11_Init(device, context)) {
        logger::error("RomantasyUI: ImGui DX11 backend init failed");
        _fonts = {};
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(_context);
        _context = nullptr;
        return false;
    }

    _ready.store(true, std::memory_order_release);
    logger::info("RomantasyUI: ImGui {} initialized ({}x{}, fonts {})", IMGUI_VERSION, desc.BufferDesc.Width, desc.BufferDesc.Height, _fonts.loaded ? "loaded" : "fallback");
    return true;
}

void ImGuiHost::RegisterInputSink()
{
    if (_sinkRegistered) {
        return;
    }
    auto* inputManager = RE::BSInputDeviceManager::GetSingleton();
    if (!inputManager) {
        logger::warn("RomantasyUI: input device manager unavailable; keyboard/mouse will not reach the dashboard");
        return;
    }
    inputManager->AddEventSink(this);
    _sinkRegistered = true;
    logger::info("RomantasyUI: ImGui input sink registered");
}

bool ImGuiHost::BeginFrame()
{
    if (!EnsureInitialized()) {
        return false;
    }
    ImGui::SetCurrentContext(_context);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();   // display size, delta time, gamepad polling
    UpdateMousePosition();         // after Win32_NewFrame so Skyrim's cursor wins
    UpdateModifiers();
    DrainPendingInput(ImGui::GetIO());
    ImGui::NewFrame();
    // Legend visibility comes exclusively from ProcessEvent, like Tailor.
    // Win32's XInput probe and synthetic cursor updates are not device changes.
    _inFrame = true;
    if (!_firstFrameLogged) {
        _firstFrameLogged = true;
        const auto size = ImGui::GetIO().DisplaySize;
        logger::info("RomantasyUI: first ImGui frame ({}x{})", size.x, size.y);
    }
    return true;
}

void ImGuiHost::EndFrame()
{
    if (!_inFrame) {
        return;
    }
    _wantsTextInput.store(ImGui::GetIO().WantTextInput, std::memory_order_release);
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    _inFrame = false;
}

void ImGuiHost::ClearInput()
{
    _wantsTextInput.store(false, std::memory_order_release);
    {
        std::scoped_lock lock(_inputLock);
        _pendingInput.clear();
    }
    if (!_ready.load(std::memory_order_acquire)) {
        return;
    }
    ImGui::SetCurrentContext(_context);
    ImGuiIO& io = ImGui::GetIO();
    io.ClearInputKeys();
    io.ClearInputMouse();
}

romantasy::ui::theme::Canvas ImGuiHost::Canvas() const
{
    return romantasy::ui::theme::Canvas::Fit(ImGui::GetIO().DisplaySize);
}

float ImGuiHost::DeltaTime() const
{
    return ImGui::GetIO().DeltaTime;
}

void ImGuiHost::UpdateMousePosition()
{
    auto* ui = RE::UI::GetSingleton();
    if (!ui || !ui->IsMenuOpen(RE::CursorMenu::MENU_NAME)) {
        return;
    }
    const auto* cursor = RE::MenuCursor::GetSingleton();
    if (!cursor) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
    io.AddMousePosEvent(cursor->GetRuntimeData().cursorPosX, cursor->GetRuntimeData().cursorPosY);
}

void ImGuiHost::UpdateModifiers()
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
    io.AddKeyEvent(ImGuiMod_Shift, (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
    io.AddKeyEvent(ImGuiMod_Alt, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
}

void ImGuiHost::DrainPendingInput(ImGuiIO& io)
{
    std::vector<PendingInput> batch;
    {
        std::scoped_lock lock(_inputLock);
        batch.swap(_pendingInput);
    }
    for (const auto& event : batch) {
        switch (event.kind) {
        case PendingInput::Kind::Key:
            io.AddKeyEvent(static_cast<ImGuiKey>(event.code), event.down);
            break;
        case PendingInput::Kind::MouseButton:
            io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
            io.AddMouseButtonEvent(event.code, event.down);
            break;
        case PendingInput::Kind::MouseWheel:
            io.AddMouseWheelEvent(0.0f, event.wheel);
            break;
        case PendingInput::Kind::Char:
            io.AddInputCharacter(static_cast<unsigned int>(event.code));
            if (!_firstCharLogged) {
                _firstCharLogged = true;
                logger::info("RomantasyUI: first character event");
            }
            break;
        }
    }
}

RE::BSEventNotifyControl ImGuiHost::ProcessEvent(
    RE::InputEvent* const* a_eventList,
    [[maybe_unused]] RE::BSTEventSource<RE::InputEvent*>* a_eventSource)
{
    if (!a_eventList) {
        return RE::BSEventNotifyControl::kContinue;
    }

    std::vector<PendingInput> batch;
    for (auto* event = *a_eventList; event; event = event->next) {
        // Remember input even while closed, so controller-opened menus and
        // passive relationship popups start with the appropriate legends.
        if (event->eventType == RE::INPUT_EVENT_TYPE::kButton) {
            const auto* button = event->AsButtonEvent();
            if (button && button->IsDown()) {
                const auto device = button->GetDevice();
                if (device == RE::INPUT_DEVICE::kGamepad) _controllerInput.SetGamepad(true);
                else if (device == RE::INPUT_DEVICE::kKeyboard || device == RE::INPUT_DEVICE::kMouse) _controllerInput.SetGamepad(false);
            }
        } else if (event->eventType == RE::INPUT_EVENT_TYPE::kMouseMove) {
            const auto* move = event->AsMouseMoveEvent();
            if (move && (move->mouseInputX != 0 || move->mouseInputY != 0)) _controllerInput.SetGamepad(false);
        } else if (event->eventType == RE::INPUT_EVENT_TYPE::kThumbstick && event->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
            const auto* stick = event->AsThumbstickEvent();
            if (stick && (std::abs(stick->xValue) > 0.25f || std::abs(stick->yValue) > 0.25f)) _controllerInput.SetGamepad(true);
        } else if (event->eventType == RE::INPUT_EVENT_TYPE::kDeviceConnect && event->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
            if (!static_cast<const RE::DeviceConnectEvent*>(event)->connected) _controllerInput.SetGamepad(false);
        } else if (event->eventType == RE::INPUT_EVENT_TYPE::kChar) {
            _controllerInput.SetGamepad(false);
        }
        if (!_ready.load(std::memory_order_acquire) || !IsActive()) continue;

        if (event->eventType == RE::INPUT_EVENT_TYPE::kChar) {
            const auto* charEvent = static_cast<const RE::CharEvent*>(event);
            if (romantasy::ui::input::IsTextCharacter(charEvent->keyCode)) {
                batch.push_back({ PendingInput::Kind::Char, static_cast<int>(charEvent->keyCode), true, 0.0f });
            }
            continue;
        }
        if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
            continue;
        }
        const auto* button = event->AsButtonEvent();
        if (!button) {
            continue;
        }
        const bool down = button->IsDown();
        const bool up = button->IsUp();
        if (!down && !up) {
            continue;  // held repeats: ImGui generates its own key repeat
        }

        switch (button->GetDevice()) {
        case RE::INPUT_DEVICE::kKeyboard: {
            const ImGuiKey key = romantasy::ui::input::ScanCodeToImGuiKey(button->GetIDCode());
            if (key != ImGuiKey_None) {
                batch.push_back({ PendingInput::Kind::Key, static_cast<int>(key), down, 0.0f });
            }
            break;
        }
        case RE::INPUT_DEVICE::kMouse: {
            const auto id = button->GetIDCode();
            if (id == kMouseLeft || id == kMouseRight || id == kMouseMiddle) {
                batch.push_back({ PendingInput::Kind::MouseButton, static_cast<int>(id), down, 0.0f });
            } else if (down && (id == kMouseWheelUp || id == kMouseWheelDown)) {
                const float notches = (std::max)(1.0f, button->Value());
                batch.push_back({ PendingInput::Kind::MouseWheel, 0, true, id == kMouseWheelUp ? notches : -notches });
            }
            break;
        }
        default:
            break;  // gamepad: the Win32 backend polls XInput itself
        }
    }

    if (!batch.empty()) {
        std::scoped_lock lock(_inputLock);
        _pendingInput.insert(_pendingInput.end(), batch.begin(), batch.end());
    }
    return RE::BSEventNotifyControl::kContinue;
}
