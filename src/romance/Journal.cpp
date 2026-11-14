#include "Journal.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace romantasy::journal
{
    namespace
    {
        constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();

        float CleanDay(float day)
        {
            return (std::isfinite(day) && day > 0.0f) ? day : 0.0f;
        }

        std::int32_t WholeDay(float cleanDay)
        {
            const float f = std::floor(cleanDay);
            return f >= static_cast<float>(kMax) ? kMax : static_cast<std::int32_t>(f);
        }

        std::int32_t ClampLevel(std::int32_t level)
        {
            return std::clamp<std::int32_t>(level, 1, 6);
        }

        std::int32_t SatAdd(std::int32_t a, std::int32_t b)
        {
            return a > kMax - b ? kMax : a + b;
        }

        std::int32_t SizeOf(std::int32_t points)
        {
            if (points >= 0) return points;
            return points == std::numeric_limits<std::int32_t>::min() ? kMax : -points;
        }

        // Cut on a byte boundary that is not a continuation byte.
        std::string CutLabel(std::string_view label)
        {
            if (label.size() <= kMaxLabelBytes) return std::string(label);
            std::size_t end = kMaxLabelBytes;
            while (end > 0 && (static_cast<unsigned char>(label[end]) & 0xC0) == 0x80) --end;
            return std::string(label.substr(0, end));
        }

        bool HasReached(const Journal& journal, std::int32_t level)
        {
            return std::any_of(journal.milestones.begin(), journal.milestones.end(), [level](const Milestone& m) {
                return m.kind == MilestoneKind::Reached && m.level == level;
            });
        }

        void AddMoment(std::vector<Change>& list, Change change, std::int32_t size)
        {
            // Largest first; a tie keeps the earlier moment ahead.
            auto at = list.begin();
            while (at != list.end() && SizeOf(at->points) >= size) ++at;
            list.insert(at, std::move(change));
            if (list.size() > kMaxMoments) list.resize(kMaxMoments);
        }

        void TrimFell(Journal& journal)
        {
            std::size_t count = 0;
            for (const auto& m : journal.milestones) {
                if (m.kind == MilestoneKind::Fell) ++count;
            }
            while (count > kMaxFell) {
                auto oldest = std::find_if(journal.milestones.begin(), journal.milestones.end(),
                    [](const Milestone& m) { return m.kind == MilestoneKind::Fell; });
                journal.milestones.erase(oldest);
                --count;
            }
        }
    }

    bool Begun(const Journal& journal)
    {
        return std::any_of(journal.milestones.begin(), journal.milestones.end(),
            [](const Milestone& m) { return m.kind == MilestoneKind::Began; });
    }

    void Begin(Journal& journal, std::int32_t level, float day)
    {
        if (Begun(journal)) return;
        const std::int32_t lvl = ClampLevel(level);
        const float d = CleanDay(day);
        journal.milestones.insert(journal.milestones.begin(),
            Milestone{ MilestoneKind::Began, static_cast<std::uint8_t>(lvl), d });
        if (lvl >= 2 && !HasReached(journal, lvl)) {
            journal.milestones.insert(journal.milestones.begin() + 1,
                Milestone{ MilestoneKind::Reached, static_cast<std::uint8_t>(lvl), d });
        }
    }

    void Record(Journal& journal, std::string_view label, std::int32_t points,
        std::int32_t oldLevel, std::int32_t newLevel, float day)
    {
        if (points == 0) return;

        const std::int32_t from = ClampLevel(oldLevel);
        const std::int32_t to = ClampLevel(newLevel);
        const float d = CleanDay(day);
        const std::int32_t whole = WholeDay(d);
        const std::int32_t size = SizeOf(points);

        Begin(journal, from, d);

        Change change{ CutLabel(label), points, d };

        journal.history.insert(journal.history.begin(), change);
        if (journal.history.size() > kMaxHistory) journal.history.resize(kMaxHistory);

        auto dayAt = std::lower_bound(journal.days.begin(), journal.days.end(), whole,
            [](const Day& entry, std::int32_t value) { return entry.day < value; });
        if (dayAt == journal.days.end() || dayAt->day != whole) {
            dayAt = journal.days.insert(dayAt, Day{ whole, 0, 0 });
        }
        if (points > 0) {
            dayAt->gained = SatAdd(dayAt->gained, size);
        } else {
            dayAt->lost = SatAdd(dayAt->lost, size);
        }
        if (journal.days.size() > kMaxDays) {
            journal.days.erase(journal.days.begin(), journal.days.end() - static_cast<std::ptrdiff_t>(kMaxDays));
        }

        if (points > 0) {
            AddMoment(journal.gains, std::move(change), size);
            for (std::int32_t level = from + 1; level <= to; ++level) {
                if (!HasReached(journal, level)) {
                    journal.milestones.push_back(
                        Milestone{ MilestoneKind::Reached, static_cast<std::uint8_t>(level), d });
                }
            }
        } else {
            AddMoment(journal.losses, std::move(change), size);
            if (to < from) {
                journal.milestones.push_back(
                    Milestone{ MilestoneKind::Fell, static_cast<std::uint8_t>(to), d });
                TrimFell(journal);
            }
        }
    }

    void RecordReset(Journal& journal, std::string_view label, std::int32_t pointsHeld, std::int32_t oldLevel, float day)
    {
        if (pointsHeld <= 0) return;

        const std::int32_t from = ClampLevel(oldLevel);
        const float d = CleanDay(day);

        Begin(journal, from, d);

        journal.history.insert(journal.history.begin(), Change{ CutLabel(label), -pointsHeld, d });
        if (journal.history.size() > kMaxHistory) journal.history.resize(kMaxHistory);

        std::erase_if(journal.milestones, [](const Milestone& m) { return m.kind == MilestoneKind::Reached; });
        if (from > 1) {
            journal.milestones.push_back(Milestone{ MilestoneKind::Fell, 1, d });
            TrimFell(journal);
        }
    }

    Journal Normalize(Journal journal)
    {
        for (auto* list : { &journal.history, &journal.gains, &journal.losses }) {
            for (auto& change : *list) {
                change.label = CutLabel(change.label);
                change.day = CleanDay(change.day);
            }
        }
        if (journal.history.size() > kMaxHistory) journal.history.resize(kMaxHistory);

        // Days: oldest first, one entry per day, no negative sums, the newest kMaxDays kept.
        for (auto& entry : journal.days) {
            entry.day = (std::max)(entry.day, 0);
            entry.gained = (std::max)(entry.gained, 0);
            entry.lost = (std::max)(entry.lost, 0);
        }
        std::stable_sort(journal.days.begin(), journal.days.end(),
            [](const Day& a, const Day& b) { return a.day < b.day; });
        std::vector<Day> days;
        days.reserve(journal.days.size());
        for (const auto& entry : journal.days) {
            if (!days.empty() && days.back().day == entry.day) {
                days.back().gained = SatAdd(days.back().gained, entry.gained);
                days.back().lost = SatAdd(days.back().lost, entry.lost);
            } else {
                days.push_back(entry);
            }
        }
        if (days.size() > kMaxDays) {
            days.erase(days.begin(), days.end() - static_cast<std::ptrdiff_t>(kMaxDays));
        }
        journal.days = std::move(days);

        // Moments: the right sign only, largest first, a tie keeping the earlier one ahead.
        std::erase_if(journal.gains, [](const Change& c) { return c.points <= 0; });
        std::erase_if(journal.losses, [](const Change& c) { return c.points >= 0; });
        for (auto* list : { &journal.gains, &journal.losses }) {
            std::stable_sort(list->begin(), list->end(),
                [](const Change& a, const Change& b) { return SizeOf(a.points) > SizeOf(b.points); });
            if (list->size() > kMaxMoments) list->resize(kMaxMoments);
        }

        // Milestones: the first Began alone and first; each tier reached once; the newest Fell kept.
        std::vector<Milestone> milestones;
        milestones.reserve(journal.milestones.size());
        std::optional<Milestone> began;
        for (auto m : journal.milestones) {
            m.day = CleanDay(m.day);
            switch (m.kind) {
            case MilestoneKind::Began:
                if (!began) {
                    m.level = static_cast<std::uint8_t>(ClampLevel(m.level));
                    began = m;
                }
                break;
            case MilestoneKind::Reached: {
                const bool seen = std::any_of(milestones.begin(), milestones.end(), [&](const Milestone& kept) {
                    return kept.kind == MilestoneKind::Reached && kept.level == m.level;
                });
                if (m.level >= 2 && m.level <= 6 && !seen) milestones.push_back(m);
                break;
            }
            case MilestoneKind::Fell:
                m.level = static_cast<std::uint8_t>(ClampLevel(m.level));
                milestones.push_back(m);
                break;
            default:
                break;
            }
        }
        if (began) milestones.insert(milestones.begin(), *began);
        journal.milestones = std::move(milestones);
        TrimFell(journal);
        return journal;
    }

    std::int32_t DayNumber(float gameDay)
    {
        if (!std::isfinite(gameDay) || gameDay < 0.0f) return 1;
        const std::int32_t whole = WholeDay(gameDay);
        return whole == kMax ? kMax : whole + 1;
    }
}
