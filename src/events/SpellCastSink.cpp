#include "events/SpellCastSink.h"

#include "settings/Settings.h"
#include "ui/RomantasyUI.h"

SpellCastSink* SpellCastSink::GetSingleton()
{
    static SpellCastSink singleton;
    return &singleton;
}

void SpellCastSink::RegisterSink()
{
    if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
        holder->AddEventSink<RE::TESSpellCastEvent>(GetSingleton());
        logger::info("Romantasy spell-cast sink registered");
    } else {
        logger::error("Romantasy: failed to get ScriptEventSourceHolder for spell-cast sink");
    }
}

RE::BSEventNotifyControl SpellCastSink::ProcessEvent(
    const RE::TESSpellCastEvent* a_event,
    RE::BSTEventSource<RE::TESSpellCastEvent>*)
{
    if (!a_event || !a_event->object) {
        return RE::BSEventNotifyControl::kContinue;
    }
    if (a_event->object.get() != RE::PlayerCharacter::GetSingleton()) {
        return RE::BSEventNotifyControl::kContinue;
    }
    if (!Settings::GetSingleton().OpenWithFavorites()) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* power = RE::TESForm::LookupByEditorID<RE::SpellItem>("ROM_PowerUI");
    if (power && a_event->spell == power->GetFormID()) {
        SKSE::GetTaskInterface()->AddTask([]() {
            RomantasyUI::GetSingleton().Toggle();
        });
    }

    return RE::BSEventNotifyControl::kContinue;
}
