#pragma once

#include "romance/Mood.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

namespace romantasy::conditions
{
    enum class Query { None, Registered, Level, Points, Mood };

    [[nodiscard]] constexpr Query ParseQuery(std::string_view name) noexcept
    {
        if (name == "Romantasy_Registered") return Query::Registered;
        if (name == "Romantasy_Level") return Query::Level;
        if (name == "Romantasy_Points") return Query::Points;
        if (name == "Romantasy_Mood") return Query::Mood;
        return Query::None;
    }

    // No result means the caller must delegate to Skyrim's original callback.
    // Level uses the public API's 1-6 numbering, not the old 0-5 faction ranks.
    [[nodiscard]] constexpr std::optional<double> Evaluate(
        Query query, std::optional<std::int32_t> points) noexcept
    {
        switch (query) {
        case Query::Registered: return points ? 1.0 : 0.0;
        case Query::Level: return points ? static_cast<double>(std::clamp(*points / 500, 0, 5) + 1) : 0.0;
        case Query::Points: return points ? static_cast<double>(*points) : -1.0;
        default: return std::nullopt;
        }
    }

    // Mood: 0 for an actor Romantasy does not manage, else 1 Strained, 2 Settled, 3 Warm
    // (Settled when the bond has no mood yet).
    [[nodiscard]] constexpr double EvaluateMood(
        std::optional<std::int32_t> points, std::optional<mood::State> state) noexcept
    {
        if (!points) {
            return 0.0;
        }
        return static_cast<double>(state.value_or(mood::State::Settled));
    }
}
