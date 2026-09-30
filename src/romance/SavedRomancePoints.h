#pragma once

#include <cstdint>
#include <unordered_map>

namespace romantasy
{
    [[nodiscard]] inline std::int32_t RestorePoints(
        const std::unordered_map<std::uint32_t, std::int32_t>& saved,
        std::uint32_t reference, std::uint32_t base, bool allowLegacyBase, std::int32_t initial)
    {
        if (const auto found = saved.find(reference); reference != 0 && found != saved.end()) {
            return found->second;
        }
        if (allowLegacyBase) {
            if (const auto found = saved.find(base); found != saved.end()) {
                return found->second;
            }
        }
        return initial;
    }
}
