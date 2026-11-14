#include "romance/PointsView.h"

#include "romance/SavedRomancePoints.h"

namespace romantasy
{
    std::optional<std::int32_t> PointsView::Find(std::uint32_t reference) const
    {
        if (reference == 0) {
            return std::nullopt;
        }
        if (const auto found = byReference.find(reference); found != byReference.end()) {
            return found->second;
        }
        return std::nullopt;
    }

    std::optional<mood::Mood> PointsView::FindMood(std::uint32_t reference) const
    {
        if (reference == 0) {
            return std::nullopt;
        }
        if (const auto found = moodByReference.find(reference); found != moodByReference.end()) {
            return found->second;
        }
        return std::nullopt;
    }

    std::optional<std::int32_t> PointsView::Query(std::uint32_t reference, std::uint32_t base, bool baseIsUnique) const
    {
        const auto profile = profiles.find(base);
        if (profile != profiles.end() && !profile->second.enabled) {
            return std::nullopt;
        }
        if (const auto points = Find(reference)) {
            return points;
        }
        if (profile != profiles.end()) {
            return RestorePoints(saved, reference, base, baseIsUnique, profile->second.startingPoints);
        }
        return std::nullopt;
    }
}
