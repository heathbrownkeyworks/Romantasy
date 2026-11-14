#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// Text for SkyrimNet: the bond line and the four event texts. Pure: standard
// headers only. These strings are placed into a language-model prompt by
// another mod's template engine, so every name goes through Safe and no output
// ever contains a brace or a percent sign.
namespace romantasy::skyrimnet
{
    struct BondFacts
    {
        std::int32_t level = 0;    // 1..6; 0 = no bond
        std::string_view tierName; // empty: the default name for the level
        std::int32_t mood = 2;     // 1 Strained, 2 Settled, 3 Warm; anything else reads as Settled
        float daysTogether = 0.0f; // non-finite or negative reads as 0; clamped at 9999
        std::string_view playerName; // the player character's name, through Safe; empty reads "the player"
    };

    // "" when level is outside 1..6.
    [[nodiscard]] std::string BondLine(const BondFacts& facts);

    // What the romantasy_bond decorator knows about one actor when SkyrimNet asks.
    struct DecoratorFacts
    {
        bool found = false;    // a usable SkyrimNet was found
        bool bridge = false;   // the "Tell SkyrimNet about bonds" setting is on
        bool bonded = false;   // the actor has a bond
        bool enabled = false;  // and that bond is switched on
        BondFacts bond;
    };

    // The bond line when every fact holds, else "".
    [[nodiscard]] std::string DecoratorLine(const DecoratorFacts& facts);

    // For names put into text. Removes { } % [ ] and invisible format characters
    // (soft hyphen, zero-width and bidi marks and overrides, BOM, tag block...).
    // Controls, line separators and Unicode spaces become a space; invalid UTF-8
    // becomes '?'. Spaces collapse, the result is trimmed, leading '#' is
    // stripped and it is cut to 64 bytes on a character boundary.
    [[nodiscard]] std::string Safe(std::string_view text);

    // The same for tier names, cut to 32 bytes.
    [[nodiscard]] std::string SafeTier(std::string_view text);

    // A name that is empty after Safe reads "Their companion"; a player name that
    // is empty after Safe reads "the player". daysApart under 1 reads "less than a
    // day"; above 9999 it reads 9999.
    [[nodiscard]] std::string TierGainedText(std::string_view name, std::string_view tier, std::string_view playerName = {});
    [[nodiscard]] std::string TierLostText(std::string_view name, std::string_view tier, std::string_view playerName = {});
    [[nodiscard]] std::string ReunionText(std::string_view name, std::int32_t daysApart, std::string_view playerName = {});
    [[nodiscard]] std::string TimeApartText(std::string_view name, std::int32_t daysApart, std::string_view playerName = {});
}
