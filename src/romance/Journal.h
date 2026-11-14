#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The bond journal: what one bond has been through. Pure: no game headers.
// It only records; it never changes points, tiers or mood.
namespace romantasy::journal
{
    struct Change
    {
        std::string label;
        std::int32_t points = 0;
        float day = 0.0f;
        bool operator==(const Change&) const = default;
    };

    // day = floor(gameDay); lost is a positive size.
    struct Day
    {
        std::int32_t day = 0;
        std::int32_t gained = 0;
        std::int32_t lost = 0;
        bool operator==(const Day&) const = default;
    };

    enum class MilestoneKind : std::uint8_t
    {
        Began = 1,
        Reached = 2,
        Fell = 3
    };

    struct Milestone
    {
        MilestoneKind kind = MilestoneKind::Began;
        std::uint8_t level = 0;
        float day = 0.0f;
        bool operator==(const Milestone&) const = default;
    };

    struct Journal
    {
        std::vector<Change> history;       // newest first
        std::vector<Day> days;             // oldest first
        std::vector<Change> gains;         // largest first
        std::vector<Change> losses;        // largest loss first (most negative points)
        std::vector<Milestone> milestones; // oldest first
        bool operator==(const Journal&) const = default;
    };

    inline constexpr std::size_t kMaxHistory = 30;
    inline constexpr std::size_t kMaxDays = 60;
    inline constexpr std::size_t kMaxMoments = 5;
    inline constexpr std::size_t kMaxFell = 20;
    inline constexpr std::size_t kMaxLabelBytes = 120;

    [[nodiscard]] bool Begun(const Journal& journal);

    // Nothing if already begun; else Began, plus Reached{level} when level is 2..6.
    // A non-finite or negative day is stored as 0; a level is clamped into 1..6.
    void Begin(Journal& journal, std::int32_t level, float day);

    // Records one applied change. points == 0 records nothing.
    void Record(Journal& journal, std::string_view label, std::int32_t points,
        std::int32_t oldLevel, std::int32_t newLevel, float day);

    // A reset: one history row with the label and the points lost, a Fell milestone to level 1
    // when the bond stood higher, no entry in the moments, nothing added to the day totals, and
    // every Reached milestone removed so the tiers can be reached again. Began stays.
    // pointsHeld <= 0 records nothing; an unbegun journal is begun first, as Record does.
    void RecordReset(Journal& journal, std::string_view label, std::int32_t pointsHeld, std::int32_t oldLevel, float day);

    // Repairs a journal read from a save into one the rules could have produced: a negative or
    // non-finite day stored as Record stores it (0, Day 1); days sorted with
    // duplicates merged and negative sums made 0; every list cut to its limit and every label to its
    // byte limit; one Began, moved first; a tier reached once and only tiers 2 to 6; levels clamped;
    // the newest Fell kept; moments of the wrong sign dropped and the rest sorted as Record keeps them.
    // A journal the rules produced comes back unchanged.
    [[nodiscard]] Journal Normalize(Journal journal);

    // floor + 1; a negative or non-finite day gives 1.
    [[nodiscard]] std::int32_t DayNumber(float gameDay);
}
