#include "input/InputMode.h"

#include "keyhandler/keyhandler.h"
#include "settings/Settings.h"
#include "ui/RomantasyUI.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <atomic>

namespace
{
    constexpr std::uint32_t kModifierScanCode = 0x1D;  // Left Control
    constexpr std::uint32_t kActivateScanCode = 0x13;  // R
    constexpr const char*   kPowerEditorID    = "ROM_PowerUI";
    constexpr RE::FormID    kPowerLocalID     = 0x83C;
    constexpr std::string_view kRomantasyPlugin = "CS_Romantasy.esp";

    KeyHandlerEvent g_hotkeyHandle = INVALID_REGISTRATION_HANDLE;

    bool IsModifierPressed()
    {
        static const auto modifierVK = MapVirtualKeyA(kModifierScanCode, MAPVK_VSC_TO_VK);
        return modifierVK != 0 && (GetAsyncKeyState(static_cast<int>(modifierVK)) & 0x8000) != 0;
    }

    void RegisterHotkey()
    {
        if (g_hotkeyHandle != INVALID_REGISTRATION_HANDLE) {
            return;
        }
        g_hotkeyHandle = KeyHandler::GetSingleton()->Register(
            kActivateScanCode, KeyEventType::KEY_DOWN, []() {
                if (!IsModifierPressed()) {
                    return;
                }
                RomantasyUI::GetSingleton().Toggle();
            });
    }

    void UnregisterHotkey()
    {
        if (g_hotkeyHandle == INVALID_REGISTRATION_HANDLE) {
            return;
        }
        KeyHandler::GetSingleton()->Unregister(g_hotkeyHandle);
        g_hotkeyHandle = INVALID_REGISTRATION_HANDLE;
    }
}

void InputMode::RefreshHotkey()
{
    if (Settings::GetSingleton().OpenWithFavorites()) {
        UnregisterHotkey();
    } else {
        RegisterHotkey();
    }
}

void InputMode::ApplyForLoadedGame()
{
    RefreshHotkey();

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

    auto* power = LookupPower();
    if (!power) {
        logger::warn("Romantasy: spell '{}' not found; cannot apply favorites mode", kPowerEditorID);
        return;
    }

    auto* magicFav = RE::MagicFavorites::GetSingleton();

    if (Settings::GetSingleton().OpenWithFavorites()) {
        if (!player->HasSpell(power)) {
            player->AddSpell(power);
            logger::info("Romantasy: granted {} to player", kPowerEditorID);
        }
        if (magicFav) {
            bool alreadyFav = false;
            for (auto* fav : magicFav->spells) {
                if (fav && fav->GetFormID() == power->GetFormID()) {
                    alreadyFav = true;
                    break;
                }
            }
            if (!alreadyFav) {
                magicFav->SetFavorite(power);
                logger::info("Romantasy: favorited {}", kPowerEditorID);
            }
        }
    } else {
        if (player->HasSpell(power)) {
            player->RemoveSpell(power);
            logger::info("Romantasy: removed {} from player", kPowerEditorID);
        }
        if (magicFav) {
            magicFav->RemoveFavorite(power);
        }
    }
}

RE::SpellItem* InputMode::LookupPower()
{
    // Forms do not move within a process: after the first find, look the power up by its form ID.
    // A failed find is not kept, so a later call searches again.
    static std::atomic<RE::FormID> cached{ 0 };
    if (const auto formID = cached.load(std::memory_order_relaxed); formID != 0) {
        return RE::TESForm::LookupByID<RE::SpellItem>(formID);
    }

    // Spells keep no editor ID unless another mod restores them, so the plugin's own form comes first.
    RE::SpellItem* power = nullptr;
    if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
        power = dataHandler->LookupForm<RE::SpellItem>(kPowerLocalID, kRomantasyPlugin);
    }
    if (!power) {
        power = RE::TESForm::LookupByEditorID<RE::SpellItem>(kPowerEditorID);
    }
    if (power) {
        cached.store(power->GetFormID(), std::memory_order_relaxed);
    }
    return power;
}

bool InputMode::PowerFound()
{
    static bool found = false;
    if (!found) {
        found = LookupPower() != nullptr;
    }
    return found;
}
