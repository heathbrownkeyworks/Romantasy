#include "keyhandler.h"

KeyHandler* KeyHandler::GetSingleton()
{
    static KeyHandler singleton;
    return &singleton;
}

void KeyHandler::RegisterSink()
{
    if (auto* inputMgr = RE::BSInputDeviceManager::GetSingleton()) {
        inputMgr->AddEventSink(GetSingleton());
        logger::info("KeyHandler sink registered");
    } else {
        logger::critical("Failed to get BSInputDeviceManager");
    }
}

KeyHandlerEvent KeyHandler::Register(std::uint32_t dxScanCode, KeyEventType eventType, KeyCallback callback)
{
    if (!callback) {
        logger::warn("Attempted to register null key callback for scan code 0x{:X}", dxScanCode);
        return INVALID_REGISTRATION_HANDLE;
    }

    const KeyHandlerEvent handle = _nextHandle.fetch_add(1);
    if (handle == INVALID_REGISTRATION_HANDLE) {
        logger::critical("KeyHandlerEvent overflow detected");
        _nextHandle.store(INVALID_REGISTRATION_HANDLE + 1);
        return INVALID_REGISTRATION_HANDLE;
    }

    std::unique_lock lock(_mutex);

    auto& keyCallbacks = _registeredCallbacks[dxScanCode];
    auto& targetMap = eventType == KeyEventType::KEY_DOWN ? keyCallbacks.down : keyCallbacks.up;
    targetMap[handle] = std::move(callback);
    _handleMap[handle] = { dxScanCode, eventType };

    logger::info("Registered key callback {} for scan code 0x{:X}", handle, dxScanCode);
    return handle;
}

void KeyHandler::Unregister(KeyHandlerEvent handle)
{
    if (handle == INVALID_REGISTRATION_HANDLE) {
        return;
    }

    std::unique_lock lock(_mutex);

    const auto handleIt = _handleMap.find(handle);
    if (handleIt == _handleMap.end()) {
        logger::warn("Attempted to unregister unknown key callback {}", handle);
        return;
    }

    const CallbackInfo info = handleIt->second;
    _handleMap.erase(handleIt);

    const auto keyCallbacksIt = _registeredCallbacks.find(info.key);
    if (keyCallbacksIt == _registeredCallbacks.end()) {
        return;
    }

    auto& keyCallbacks = keyCallbacksIt->second;
    auto& targetMap = info.type == KeyEventType::KEY_DOWN ? keyCallbacks.down : keyCallbacks.up;
    targetMap.erase(handle);

    if (keyCallbacks.down.empty() && keyCallbacks.up.empty()) {
        _registeredCallbacks.erase(keyCallbacksIt);
    }
}

RE::BSEventNotifyControl KeyHandler::ProcessEvent(
    RE::InputEvent* const* a_eventList,
    [[maybe_unused]] RE::BSTEventSource<RE::InputEvent*>* a_eventSource)
{
    if (!a_eventList) {
        return RE::BSEventNotifyControl::kContinue;
    }

    std::vector<KeyCallback> callbacksToRun;

    for (auto* event = *a_eventList; event; event = event->next) {
        if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
            continue;
        }

        const auto* buttonEvent = event->AsButtonEvent();
        if (!buttonEvent || buttonEvent->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
            continue;
        }

        KeyEventType eventType;
        if (buttonEvent->IsDown()) {
            eventType = KeyEventType::KEY_DOWN;
        } else if (buttonEvent->IsUp()) {
            eventType = KeyEventType::KEY_UP;
        } else {
            continue;
        }

        std::shared_lock lock(_mutex);
        const auto keyCallbacksIt = _registeredCallbacks.find(buttonEvent->GetIDCode());
        if (keyCallbacksIt == _registeredCallbacks.end()) {
            continue;
        }

        const auto& keyCallbacks = keyCallbacksIt->second;
        const auto& targetMap = eventType == KeyEventType::KEY_DOWN ? keyCallbacks.down : keyCallbacks.up;
        for (const auto& [handle, callback] : targetMap) {
            callbacksToRun.push_back(callback);
        }
    }

    for (const auto& callback : callbacksToRun) {
        callback();
    }

    return RE::BSEventNotifyControl::kContinue;
}
