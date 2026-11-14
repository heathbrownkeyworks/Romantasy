#include "Why.h"

#include <cmath>
#include <cstddef>
#include <charconv>

namespace romantasy::why
{
    namespace
    {
        // Length of the valid UTF-8 sequence at s[i], or 0 when it is not valid.
        std::size_t SequenceLength(std::string_view s, std::size_t i)
        {
            const auto b = [&](std::size_t k) { return static_cast<unsigned char>(s[k]); };
            const unsigned char c = b(i);
            std::size_t n = 0;
            if (c < 0x80) return 1;
            if (c >= 0xC2 && c <= 0xDF) n = 2;
            else if (c >= 0xE0 && c <= 0xEF) n = 3;
            else if (c >= 0xF0 && c <= 0xF4) n = 4;
            else return 0;
            if (i + n > s.size()) return 0;
            for (std::size_t k = 1; k < n; ++k) {
                if ((b(i + k) & 0xC0) != 0x80) return 0;
            }
            const unsigned char c1 = b(i + 1);
            if (c == 0xE0 && c1 < 0xA0) return 0; // overlong
            if (c == 0xED && c1 > 0x9F) return 0; // surrogate
            if (c == 0xF0 && c1 < 0x90) return 0; // overlong
            if (c == 0xF4 && c1 > 0x8F) return 0; // above U+10FFFF
            return n;
        }

        bool IsControl(std::string_view text, std::size_t i, std::size_t n)
        {
            const auto c = static_cast<unsigned char>(text[i]);
            if (c < 0x20 || c == 0x7F || c == '|') return true;
            // C1 controls U+0080..U+009F.
            if (n == 2 && c == 0xC2 && static_cast<unsigned char>(text[i + 1]) < 0xA0) return true;
            // Line and paragraph separators U+2028, U+2029.
            if (n == 3 && c == 0xE2 && static_cast<unsigned char>(text[i + 1]) == 0x80) {
                const auto c2 = static_cast<unsigned char>(text[i + 2]);
                return c2 == 0xA8 || c2 == 0xA9;
            }
            return false;
        }

        // One safe, short piece of text: control characters and | become spaces,
        // invalid bytes become '?', and the cut never splits a character.
        std::string Clean(std::string_view text, std::size_t limit = kMaxTextBytes)
        {
            std::string out;
            std::size_t i = 0;
            while (i < text.size()) {
                const std::size_t n = SequenceLength(text, i);
                const std::size_t used = n == 0 ? 1 : n;
                const bool replaced = n == 0 || IsControl(text, i, n);
                if (out.size() + (replaced ? 1 : n) > limit) break;
                if (n == 0) out += '?';
                else if (replaced) out += ' ';
                else out.append(text.substr(i, n));
                i += used;
            }
            return out;
        }

        std::string Signed(std::int32_t value)
        {
            const std::int64_t v = value;
            return (v < 0 ? "-" : "+") + std::to_string(v < 0 ? -v : v);
        }

        std::string Weight(std::int32_t weight)
        {
            switch (weight) {
            case -2: return "pet peeve x2";
            case -1: return "dislike";
            case 1: return "like";
            case 2: return "favorite x2";
            default: return "weight " + std::to_string(weight);
            }
        }

        // Empty for standard pace, which the line leaves out.
        std::string Pace(std::int32_t pace)
        {
            switch (pace) {
            case -1: return "slow /2";
            case 0: return {};
            case 1: return "fast x2";
            default: return "pace " + std::to_string(pace);
            }
        }

        std::string Days(float days)
        {
            if (!std::isfinite(days) || !(days > 0.0f)) days = 0.0f;
            char buffer[96];
            const auto result = std::to_chars(buffer, buffer + sizeof(buffer), days, std::chars_format::fixed, 1);
            return std::string(buffer, result.ptr);
        }

        std::string Head(std::string_view name, std::string_view deed, std::int32_t count)
        {
            return "why: " + Clean(name) + " | " + Clean(deed) + " x" + std::to_string(count);
        }

        std::string Movement(std::int32_t applied, std::int32_t oldPoints, std::int32_t newPoints, std::string_view tier)
        {
            std::string out = Signed(applied) + " | " + std::to_string(oldPoints) + " -> " + std::to_string(newPoints);
            const std::string cleanTier = Clean(tier);
            if (!cleanTier.empty()) out += " " + cleanTier;
            return out;
        }
    }

    std::string Changed(const Deed& deed)
    {
        std::string out = Head(deed.name, deed.deed, deed.count) + " | rule " + std::to_string(deed.rule) + " | " + Weight(deed.weight);
        const std::string pace = Pace(deed.pace);
        if (!pace.empty()) out += " | " + pace;
        return out + " | " + Movement(deed.applied, deed.oldPoints, deed.newPoints, deed.tier);
    }

    std::optional<Nothing> Reason(const Outcome& outcome)
    {
        if (!outcome.feeling) return Nothing::NoFeeling;
        if (outcome.points == 0) return Nothing::NoRule;
        if (!outcome.following) return Nothing::NotFollowing;
        if (outcome.paced == 0) return Nothing::RoundedToZero;
        if (outcome.refused) return Nothing::Gated;
        if (outcome.applied == 0) return Nothing::AtLimit;
        return std::nullopt;
    }

    std::int32_t PaceShown(std::int32_t pace, std::int32_t points)
    {
        return points > 0 ? pace : 0;
    }

    std::string CleanText(std::string_view text, std::size_t limit)
    {
        return Clean(text, limit);
    }

    namespace
    {
        std::string ReasonText(Nothing reason, std::string_view gateTier, float daysSoFar, std::int32_t daysNeeded)
        {
            std::string text;
            switch (reason) {
            case Nothing::NotFollowing: text = "not following"; break;
            case Nothing::NoFeeling: text = "no feeling about this deed"; break;
            case Nothing::NoRule: text = "no rule for this deed"; break;
            case Nothing::Gated: {
                const std::string tier = Clean(gateTier);
                text = std::string("stopped by the ") + (tier.empty() ? "" : tier + " ") + "gate (" + Days(daysSoFar) + " of " +
                       std::to_string(daysNeeded) + " days)";
                break;
            }
            case Nothing::RoundedToZero: text = "rounded to 0 by slow burn"; break;
            case Nothing::AtLimit: text = "already at the limit"; break;
            default: text = "unknown reason"; break;
            }
            return text;
        }
    }

    std::string NothingText(Nothing reason, std::string_view gateTier, float daysSoFar, std::int32_t daysNeeded)
    {
        return ReasonText(reason, gateTier, daysSoFar, daysNeeded);
    }

    std::string Unchanged(std::string_view name, std::string_view deed, std::int32_t count, Nothing reason,
                          std::string_view gateTier, float daysSoFar, std::int32_t daysNeeded)
    {
        return Head(name, deed, count) + " | nothing: " + ReasonText(reason, gateTier, daysSoFar, daysNeeded);
    }

    std::string ScriptUnchanged(std::string_view name, Nothing reason, std::string_view gateTier, float daysSoFar,
                                std::int32_t daysNeeded)
    {
        return "why: " + Clean(name) + " | script award | nothing: " + ReasonText(reason, gateTier, daysSoFar, daysNeeded);
    }

    std::string Script(std::string_view name, std::int32_t applied, std::int32_t oldPoints, std::int32_t newPoints, std::string_view tier)
    {
        return "why: " + Clean(name) + " | script award | " + Movement(applied, oldPoints, newPoints, tier);
    }

    std::string Moment(std::string_view name, std::int32_t applied, std::int32_t oldPoints, std::int32_t newPoints, std::string_view tier)
    {
        return "why: " + Clean(name) + " | shared moment | " + Movement(applied, oldPoints, newPoints, tier);
    }

    std::string MomentNothing(std::string_view name, std::string_view reason)
    {
        return "why: " + Clean(name) + " | shared moment | nothing: " + Clean(reason);
    }

    std::string SceneNothing(std::string_view reason)
    {
        return "why: scene | shared moment | nothing: " + Clean(reason);
    }
}
