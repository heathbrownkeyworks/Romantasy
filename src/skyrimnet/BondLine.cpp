#include "skyrimnet/BondLine.h"

#include <cmath>
#include <cstddef>

namespace romantasy::skyrimnet
{
    namespace
    {
        constexpr std::size_t kMaxNameBytes = 64, kMaxTierBytes = 32;
        constexpr std::int32_t kMaxDays = 9999;
        constexpr const char* kFallbackName = "Their companion"; // every text starts with the name
        constexpr const char* kFallbackPlayer = "the player";    // the player's name when it is empty after Safe

        // Length of the valid UTF-8 sequence at text[i], or 0 when invalid.
        // Rejects overlong forms, surrogates and values above U+10FFFF.
        std::size_t SequenceLength(std::string_view text, std::size_t i, std::uint32_t& cp)
        {
            const auto b = [&](std::size_t k) { return static_cast<std::uint32_t>(static_cast<unsigned char>(text[k])); };
            const auto cont = [&](std::size_t k) { return k < text.size() && (b(k) & 0xC0u) == 0x80u; };
            const std::uint32_t c = b(i);
            if (c < 0x80) {
                cp = c;
                return 1;
            }
            if (c >= 0xC2 && c <= 0xDF && cont(i + 1)) {
                cp = ((c & 0x1Fu) << 6) | (b(i + 1) & 0x3Fu);
                return 2;
            }
            if (c >= 0xE0 && c <= 0xEF && cont(i + 1) && cont(i + 2)) {
                cp = ((c & 0x0Fu) << 12) | ((b(i + 1) & 0x3Fu) << 6) | (b(i + 2) & 0x3Fu);
                if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
                return 3;
            }
            if (c >= 0xF0 && c <= 0xF4 && cont(i + 1) && cont(i + 2) && cont(i + 3)) {
                cp = ((c & 0x07u) << 18) | ((b(i + 1) & 0x3Fu) << 12) | ((b(i + 2) & 0x3Fu) << 6) | (b(i + 3) & 0x3Fu);
                if (cp < 0x10000 || cp > 0x10FFFF) return 0;
                return 4;
            }
            return 0;
        }

        // Becomes a space: controls, line separators and Unicode space separators.
        bool SpaceLike(std::uint32_t cp)
        {
            return cp <= 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F) || cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) ||
                   cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
        }

        // Removed outright: template syntax and invisible format characters.
        bool Dropped(std::uint32_t cp)
        {
            return cp == '{' || cp == '}' || cp == '%' || cp == '[' || cp == ']' || cp == 0xAD || cp == 0x61C || cp == 0x180E || (cp >= 0x200B && cp <= 0x200F) ||
                   (cp >= 0x202A && cp <= 0x202E) || (cp >= 0x2060 && cp <= 0x206F) || cp == 0xFEFF || (cp >= 0xFFF9 && cp <= 0xFFFB) ||
                   (cp >= 0xE0000 && cp <= 0xE007F);
        }

        std::string Trimmed(const std::string& s)
        {
            const std::size_t a = s.find_first_not_of(' ');
            if (a == std::string::npos) return {};
            return s.substr(a, s.find_last_not_of(' ') - a + 1);
        }

        std::string Name(std::string_view name)
        {
            std::string s = Safe(name);
            return s.empty() ? std::string(kFallbackName) : s;
        }

        std::string Player(std::string_view playerName)
        {
            std::string s = Safe(playerName);
            return s.empty() ? std::string(kFallbackPlayer) : s;
        }

        std::string DaysApart(std::int32_t days)
        {
            if (days < 1) return "less than a day";
            if (days > kMaxDays) days = kMaxDays;
            return std::to_string(days) + (days == 1 ? " day" : " days");
        }

        const char* DefaultTier(std::int32_t level)
        {
            switch (level) {
            case 1: return "Stranger";
            case 2: return "Acquaintance";
            case 3: return "Friend";
            case 4: return "Confidant";
            case 5: return "Lover";
            default: return "Spouse";
            }
        }

        std::string TierSentence(std::int32_t level, const std::string& player)
        {
            switch (level) {
            case 1: return "They barely know each other.";
            case 2: return "Familiar and polite; not yet close.";
            case 3: return "A trusted friend; warmth without romance.";
            case 4: return "Close enough to share private thoughts; an unspoken attraction may be growing.";
            case 5: return "In a romantic relationship with " + player + ".";
            default: return "Married to " + player + ".";
            }
        }
    }

    namespace
    {
        std::string Clean(std::string_view text, std::size_t maxBytes)
        {
            std::string out;
            out.reserve(text.size());
            bool lastSpace = true; // collapses runs and drops leading spaces
            for (std::size_t i = 0; i < text.size();) {
                std::uint32_t cp = 0;
                const std::size_t n = SequenceLength(text, i, cp);
                if (n == 0) {
                    out.push_back('?');
                    lastSpace = false;
                    ++i;
                    continue;
                }
                if (Dropped(cp)) {
                    i += n;
                    continue;
                }
                if (SpaceLike(cp)) {
                    if (!lastSpace) out.push_back(' ');
                    lastSpace = true;
                } else {
                    out.append(text.substr(i, n));
                    lastSpace = false;
                }
                i += n;
            }
            // No leading '#', so a name can never start a line with a heading mark.
            const std::size_t first = out.find_first_not_of("# ");
            out.erase(0, first == std::string::npos ? out.size() : first);
            if (out.size() > maxBytes) {
                std::size_t cut = maxBytes;
                while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) --cut;
                out.resize(cut);
            }
            return Trimmed(out);
        }
    }

    std::string Safe(std::string_view text)
    {
        return Clean(text, kMaxNameBytes);
    }

    std::string SafeTier(std::string_view text)
    {
        return Clean(text, kMaxTierBytes);
    }

    std::string DecoratorLine(const DecoratorFacts& facts)
    {
        if (!facts.found || !facts.bridge || !facts.bonded || !facts.enabled) return {};
        return BondLine(facts.bond);
    }

    std::string BondLine(const BondFacts& facts)
    {
        if (facts.level < 1 || facts.level > 6) return {};
        std::string tier = SafeTier(facts.tierName);
        if (tier.empty()) tier = DefaultTier(facts.level);

        const std::string player = Player(facts.playerName);
        std::string line = "Romantasy bond with " + player + ": " + tier + " (tier " + std::to_string(facts.level) + " of 6). " + TierSentence(facts.level, player);
        if (facts.mood == 3) line += " Mood toward " + player + " right now: Warm.";
        else if (facts.mood == 1) line += " Things are strained between them right now.";

        if (std::isfinite(facts.daysTogether) && facts.daysTogether >= 1.0f) {
            const double whole = std::floor(static_cast<double>(facts.daysTogether));
            const std::int32_t days = whole >= kMaxDays ? kMaxDays : static_cast<std::int32_t>(whole);
            line += " They have spent " + std::to_string(days) + (days == 1 ? " day" : " days") + " travelling together.";
        }
        return line;
    }

    std::string TierGainedText(std::string_view name, std::string_view tier, std::string_view playerName)
    {
        std::string t = SafeTier(tier);
        if (t.empty()) t = "a new tier";
        return Name(name) + "'s bond with " + Player(playerName) + " deepened: they are now " + t + ".";
    }

    std::string TierLostText(std::string_view name, std::string_view tier, std::string_view playerName)
    {
        std::string t = SafeTier(tier);
        if (t.empty()) t = "a lower tier";
        return Name(name) + "'s bond with " + Player(playerName) + " was damaged: they have fallen back to " + t + ".";
    }

    std::string ReunionText(std::string_view name, std::int32_t daysApart, std::string_view playerName)
    {
        return Name(name) + " and " + Player(playerName) + " are together again after " + DaysApart(daysApart) + " apart.";
    }

    std::string TimeApartText(std::string_view name, std::int32_t daysApart, std::string_view playerName)
    {
        return Name(name) + " has not travelled with " + Player(playerName) + " for " + DaysApart(daysApart) + ".";
    }
}
