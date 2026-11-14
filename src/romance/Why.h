#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// One "why" log line for a points decision. Pure: no game headers. It only
// builds text; it never changes points, tiers or mood. Every line is one line:
// names and labels come from mods, so control characters and | become spaces,
// invalid UTF-8 becomes '?' and each is cut to kMaxTextBytes.
namespace romantasy::why
{
    inline constexpr std::size_t kMaxTextBytes = 80;

    enum class Nothing : std::uint8_t
    {
        NotFollowing,
        NoFeeling,
        NoRule,
        Gated,
        RoundedToZero,
        AtLimit   // the change was allowed but the total could not move (a loss at 0)
    };

    // What one points decision found, in plain values.
    struct Outcome
    {
        bool feeling = true;              // the bond has a preference for the deed
        std::int32_t points = 0;          // the preference's points before pace; 0 means the rule gives none
        bool following = true;            // following, or not required to be
        std::int32_t paced = 0;           // the points after pace; 0 means rounded away
        bool refused = false;             // a Slow burn gate held all of it
        std::int32_t applied = 0;         // the new total minus the old total
    };

    // Why nothing changed, or nullopt when the total moved. First match wins, in this order:
    // no feeling, no rule, not following, rounded to 0, stopped by a gate, at the limit.
    [[nodiscard]] std::optional<Nothing> Reason(const Outcome& outcome);

    // The pace a line names: only a gain is paced, so a loss (or no change) names none (0).
    [[nodiscard]] std::int32_t PaceShown(std::int32_t pace, std::int32_t points);

    // One safe piece of mod text: control characters and | become spaces, invalid UTF-8
    // becomes '?', and the text is cut at `limit` bytes without splitting a character.
    [[nodiscard]] std::string CleanText(std::string_view text, std::size_t limit = kMaxTextBytes);

    struct Deed
    {
        std::string_view name, deed;      // follower name, deed label
        std::int32_t count = 1, rule = 0; // event count, the rule's points per event
        std::int32_t weight = 0;          // -2 pet peeve, -1 dislike, 1 like, 2 favorite
        std::int32_t pace = 0;            // -1 slow, 0 standard, 1 fast
        std::int32_t applied = 0;         // the change that was applied
        std::int32_t oldPoints = 0, newPoints = 0;
        std::string_view tier;            // tier name after the change
    };

    // A weight or pace outside the listed values prints "weight N" / "pace N".
    // A non-finite or negative daysSoFar prints as 0.0.
    [[nodiscard]] std::string Changed(const Deed& deed);
    [[nodiscard]] std::string Unchanged(std::string_view name, std::string_view deed, std::int32_t count, Nothing reason,
                                        std::string_view gateTier = {}, float daysSoFar = 0.0f, std::int32_t daysNeeded = 0);
    [[nodiscard]] std::string Script(std::string_view name, std::int32_t applied, std::int32_t oldPoints, std::int32_t newPoints, std::string_view tier);
    // A script award that changed nothing: "why: {Name} | script award | nothing: {reason}".
    [[nodiscard]] std::string ScriptUnchanged(std::string_view name, Nothing reason,
                                              std::string_view gateTier = {}, float daysSoFar = 0.0f, std::int32_t daysNeeded = 0);
    // The reason text a "nothing" line gives, as Unchanged and ScriptUnchanged write it after "nothing: ".
    [[nodiscard]] std::string NothingText(Nothing reason, std::string_view gateTier = {}, float daysSoFar = 0.0f, std::int32_t daysNeeded = 0);
    // A shared moment that moved the total: "why: {Name} | shared moment | +5 | 995 -> 1000 Friend".
    [[nodiscard]] std::string Moment(std::string_view name, std::int32_t applied, std::int32_t oldPoints, std::int32_t newPoints, std::string_view tier);
    // A shared moment that gave nothing: "why: {Name} | shared moment | nothing: {reason}".
    [[nodiscard]] std::string MomentNothing(std::string_view name, std::string_view reason);
    // A scene that counted for no bond at all: "why: scene | shared moment | nothing: {reason}".
    [[nodiscard]] std::string SceneNothing(std::string_view reason);
}
