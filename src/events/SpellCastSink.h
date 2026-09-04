#pragma once

// Listens for the player casting ROM_PowerUI (favorites-menu open) and toggles
// the Romantasy panel. Registered unconditionally; gated on Settings + spell match.
class SpellCastSink : public RE::BSTEventSink<RE::TESSpellCastEvent>
{
public:
    static SpellCastSink* GetSingleton();
    static void RegisterSink();

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESSpellCastEvent* a_event,
        RE::BSTEventSource<RE::TESSpellCastEvent>* a_source) override;

private:
    SpellCastSink() = default;
    ~SpellCastSink() override = default;
    SpellCastSink(const SpellCastSink&) = delete;
    SpellCastSink(SpellCastSink&&) = delete;
    SpellCastSink& operator=(const SpellCastSink&) = delete;
    SpellCastSink& operator=(SpellCastSink&&) = delete;
};
