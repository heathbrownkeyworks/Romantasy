#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

namespace romantasy::conditions
{
    enum class Query { None, Registered, Level, Points };

    [[nodiscard]] constexpr Query ParseQuery(std::string_view name) noexcept
    {
        if (name == "Romantasy_Registered") return Query::Registered;
        if (name == "Romantasy_Level") return Query::Level;
        if (name == "Romantasy_Points") return Query::Points;
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
}
