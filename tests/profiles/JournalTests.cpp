#include "check.h"
#include "romance/Journal.h"

#include <cstdint>
#include <limits>
#include <string>

using namespace romantasy::journal;

namespace
{
    constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
    constexpr std::int32_t kMin = std::numeric_limits<std::int32_t>::min();
    constexpr float kNan = std::numeric_limits<float>::quiet_NaN();
    constexpr float kInf = std::numeric_limits<float>::infinity();

    std::size_t Count(const Journal& j, MilestoneKind kind)
    {
        std::size_t n = 0;
        for (const auto& m : j.milestones) {
            if (m.kind == kind) ++n;
        }
        return n;
    }

    bool Has(const Journal& j, MilestoneKind kind, int level)
    {
        for (const auto& m : j.milestones) {
            if (m.kind == kind && m.level == level) return true;
        }
        return false;
    }
}

TEST(Journal_BeginsOnce)
{
    Journal a;
    CHECK(!Begun(a));
    Begin(a, 3, 5.5f);
    CHECK(Begun(a));
    CHECK(a.milestones.size() == 2);
    CHECK(a.milestones[0].kind == MilestoneKind::Began);
    CHECK(a.milestones[0].day == 5.5f);
    CHECK(a.milestones[1].kind == MilestoneKind::Reached);
    CHECK(a.milestones[1].level == 3);

    Journal b;
    Begin(b, 1, 0.0f);
    CHECK(b.milestones.size() == 1);
    CHECK(b.milestones[0].kind == MilestoneKind::Began);

    const Journal before = a;
    Begin(a, 5, 99.0f);
    CHECK(a == before);

    Journal c;
    Begin(c, 99, 1.0f);
    CHECK(Has(c, MilestoneKind::Reached, 6));
    Journal d;
    Begin(d, -4, 1.0f);
    CHECK(d.milestones.size() == 1);
    CHECK(d.milestones[0].level == 1);
}

TEST(Journal_HistoryNewestFirstAndCapped)
{
    Journal j;
    for (int i = 1; i <= 31; ++i) {
        Record(j, "c" + std::to_string(i), i, 1, 1, 1.0f);
    }
    CHECK(j.history.size() == kMaxHistory);
    CHECK(j.history.front().label == "c31");
    CHECK(j.history.back().label == "c2");
    for (const auto& c : j.history) CHECK(c.label != "c1");
}

TEST(Journal_DaysMergeAndCap)
{
    Journal j;
    Record(j, "a", 10, 1, 1, 4.2f);
    Record(j, "b", 5, 1, 1, 4.9f);
    CHECK(j.days.size() == 1);
    CHECK((j.days[0] == Day{ 4, 15, 0 }));
    Record(j, "c", -7, 1, 1, 4.5f);
    CHECK(j.days.size() == 1);
    CHECK((j.days[0] == Day{ 4, 15, 7 }));

    Journal k;
    for (int d = 0; d < 61; ++d) Record(k, "x", 1, 1, 1, static_cast<float>(d) + 0.5f);
    CHECK(k.days.size() == kMaxDays);
    CHECK(k.days.front().day == 1);
    CHECK(k.days.back().day == 60);
}

TEST(Journal_DaysStaySortedWhenClockGoesBack)
{
    Journal j;
    Record(j, "a", 1, 1, 1, 10.0f);
    Record(j, "b", 2, 1, 1, 12.0f);
    Record(j, "c", 3, 1, 1, 11.0f);
    Record(j, "d", 4, 1, 1, 10.5f);
    CHECK(j.days.size() == 3);
    CHECK((j.days[0] == Day{ 10, 5, 0 }));
    CHECK((j.days[1] == Day{ 11, 3, 0 }));
    CHECK((j.days[2] == Day{ 12, 2, 0 }));
    CHECK(j.history.front().label == "d");
    CHECK(j.history.size() == 4);
}

TEST(Journal_NonFiniteDaysStoreZero)
{
    Journal j;
    Record(j, "a", 1, 2, 2, kNan);
    Record(j, "b", 1, 2, 2, kInf);
    Record(j, "c", 1, 2, 2, -kInf);
    Record(j, "d", 1, 2, 2, -3.0f);
    CHECK(j.days.size() == 1);
    CHECK(j.days[0].day == 0);
    CHECK(j.days[0].gained == 4);
    for (const auto& c : j.history) CHECK(c.day == 0.0f);
    for (const auto& m : j.milestones) CHECK(m.day == 0.0f);
    Journal k;
    Begin(k, 1, kNan);
    CHECK(k.milestones[0].day == 0.0f);
}

TEST(Journal_MomentsKeepTopFive)
{
    Journal j;
    const int sizes[] = { 3, 9, 1, 7, 5, 8 };
    for (int s : sizes) Record(j, "g" + std::to_string(s), s, 1, 1, 1.0f);
    CHECK(j.gains.size() == kMaxMoments);
    CHECK(j.gains[0].points == 9);
    CHECK(j.gains[1].points == 8);
    CHECK(j.gains[2].points == 7);
    CHECK(j.gains[3].points == 5);
    CHECK(j.gains[4].points == 3);

    Journal t;
    Record(t, "first", 5, 1, 1, 1.0f);
    Record(t, "second", 5, 1, 1, 1.0f);
    CHECK(t.gains[0].label == "first");
    CHECK(t.gains[1].label == "second");
    for (int i = 0; i < 5; ++i) Record(t, "late", 5, 1, 1, 1.0f);
    CHECK(t.gains.size() == kMaxMoments);
    CHECK(t.gains[0].label == "first");
    CHECK(t.gains[1].label == "second");
    CHECK(t.gains[2].label == "late");

    Journal l;
    const int drops[] = { -3, -9, -1, -7, -5, -8 };
    for (int s : drops) Record(l, "l", s, 1, 1, 1.0f);
    CHECK(l.losses.size() == kMaxMoments);
    CHECK(l.losses[0].points == -9);
    CHECK(l.losses[1].points == -8);
    CHECK(l.losses[4].points == -3);
    Journal m;
    Record(m, "first", -4, 1, 1, 1.0f);
    Record(m, "second", -4, 1, 1, 1.0f);
    CHECK(m.losses[0].label == "first");
    CHECK(m.gains.empty());
}

TEST(Journal_ReachedOncePerTier)
{
    Journal j;
    Record(j, "up", 900, 1, 3, 2.0f);
    CHECK(Has(j, MilestoneKind::Reached, 2));
    CHECK(Has(j, MilestoneKind::Reached, 3));
    CHECK(Count(j, MilestoneKind::Reached) == 2);
    CHECK(Count(j, MilestoneKind::Began) == 1);

    Record(j, "down", -300, 3, 2, 3.0f);
    CHECK(Has(j, MilestoneKind::Fell, 2));
    CHECK(Count(j, MilestoneKind::Fell) == 1);

    Record(j, "up again", 300, 2, 3, 4.0f);
    CHECK(Count(j, MilestoneKind::Reached) == 2);
    CHECK(Count(j, MilestoneKind::Fell) == 1);

    // A loss inside a tier adds no Fell.
    Record(j, "small", -5, 3, 3, 5.0f);
    CHECK(Count(j, MilestoneKind::Fell) == 1);
    // A bad level is clamped, not stored.
    Record(j, "odd", 5, 0, 99, 6.0f);
    for (const auto& m : j.milestones) CHECK(m.level >= 1 && m.level <= 6);
}

TEST(Journal_FellCappedAtTwenty)
{
    Journal j;
    Record(j, "start", 1500, 1, 4, 1.0f);
    for (int i = 0; i < 25; ++i) {
        Record(j, "fall", -10, 4, 3, 10.0f + static_cast<float>(i));
        Record(j, "rise", 10, 3, 4, 10.5f + static_cast<float>(i));
    }
    CHECK(Count(j, MilestoneKind::Fell) == kMaxFell);
    CHECK(Count(j, MilestoneKind::Began) == 1);
    CHECK(Count(j, MilestoneKind::Reached) == 3);
    // The oldest five Fell are gone: the first kept one is from loop i == 5.
    for (const auto& m : j.milestones) {
        if (m.kind == MilestoneKind::Fell) {
            CHECK(m.day == 15.0f);
            break;
        }
    }
    CHECK(j.milestones.back().kind == MilestoneKind::Fell);
    CHECK(j.milestones.back().day == 34.0f);
}

TEST(Journal_LabelCut)
{
    std::string label;
    for (int i = 0; i < 100; ++i) label += "\xC3\xA9"; // 200 bytes
    Journal j;
    Record(j, label, 1, 1, 1, 1.0f);
    const std::string& cut = j.history[0].label;
    CHECK(cut.size() <= kMaxLabelBytes);
    CHECK(cut.size() == 120);
    CHECK(cut.size() % 2 == 0);
    CHECK((static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80);

    // Three-byte characters: 120 is a boundary; offset by one ASCII byte it is not.
    std::string odd = "a";
    for (int i = 0; i < 80; ++i) odd += "\xE2\x82\xAC";
    Record(j, odd, 1, 1, 1, 1.0f);
    const std::string& cut2 = j.history[0].label;
    CHECK(cut2.size() == 118);
    CHECK(cut2.back() == '\xAC');

    // Invalid: only continuation bytes. No loop, no underflow.
    Record(j, std::string(200, '\x80'), 1, 1, 1, 1.0f);
    CHECK(j.history[0].label.empty());

    // Short labels are untouched.
    Record(j, "short", 1, 1, 1, 1.0f);
    CHECK(j.history[0].label == "short");
}

TEST(Journal_ZeroRecordsNothing)
{
    Journal j;
    Record(j, "nothing", 0, 1, 1, 1.0f);
    CHECK(j == Journal{});
    CHECK(!Begun(j));
}

TEST(Journal_ExtremePointsDoNotOverflow)
{
    Journal j;
    Record(j, "huge loss", kMin, 3, 1, 1.0f);
    CHECK(j.days[0].lost == kMax);
    Record(j, "more", -5, 1, 1, 1.0f);
    CHECK(j.days[0].lost == kMax);
    Record(j, "huge gain", kMax, 1, 1, 1.0f);
    Record(j, "more", kMax, 1, 1, 1.0f);
    CHECK(j.days[0].gained == kMax);
    CHECK(j.losses[0].points == kMin);
    CHECK(j.losses.size() == 2);
    CHECK(j.losses[1].points == -5);
}

TEST(Journal_DayNumber)
{
    CHECK(DayNumber(0.0f) == 1);
    CHECK(DayNumber(211.9f) == 212);
    CHECK(DayNumber(-3.0f) == 1);
    CHECK(DayNumber(kNan) == 1);
    CHECK(DayNumber(kInf) == 1);
    CHECK(DayNumber(-kInf) == 1);
    CHECK(DayNumber(1e30f) == kMax);
}

// Normalize: a journal read from a save is repaired into one the rules could have produced.

TEST(JournalNormalize_RulesJournalUnchanged)
{
    Journal j;
    Begin(j, 1, 0.5f);
    Record(j, "start", 1500, 1, 4, 1.0f);
    for (int i = 0; i < 70; ++i) {
        const float day = 2.0f + static_cast<float>(i);
        Record(j, "fall " + std::to_string(i), -(10 + i % 7), 4, 3, day);
        Record(j, "rise " + std::to_string(i), 10 + i % 5, 3, 4, day + 0.5f);
        Record(j, "same day", 5, 4, 4, day + 0.6f);
    }
    Record(j, "tie a", 5, 4, 4, 90.0f);
    Record(j, "tie b", 5, 4, 4, 90.0f);
    CHECK(Normalize(j) == j);
    CHECK(Normalize(Journal{}) == Journal{});
}

TEST(JournalNormalize_DaysSortedAndMerged)
{
    Journal j;
    j.days = { { 7, 5, 0 }, { 3, 1, 2 }, { 7, 2, 4 }, { 5, 0, 1 } };
    const auto n = Normalize(j);
    CHECK(n.days.size() == 3);
    CHECK(n.days[0] == (Day{ 3, 1, 2 }));
    CHECK(n.days[1] == (Day{ 5, 0, 1 }));
    CHECK(n.days[2] == (Day{ 7, 7, 4 }));

    Journal big;
    big.days = { { 1, kMax, 0 }, { 1, 5, 0 } };
    CHECK(Normalize(big).days[0].gained == kMax);
}

TEST(JournalNormalize_NegativeDaySumsClamped)
{
    Journal j;
    j.days = { { 1, -5, 3 }, { 2, 4, -1 }, { 2, -10, 2 } };
    const auto n = Normalize(j);
    CHECK(n.days.size() == 2);
    CHECK(n.days[0] == (Day{ 1, 0, 3 }));
    CHECK(n.days[1] == (Day{ 2, 4, 2 }));
}

TEST(JournalNormalize_ListsCapped)
{
    Journal j;
    for (int i = 0; i < 40; ++i) j.history.push_back({ "h" + std::to_string(i), 1, static_cast<float>(i) });
    for (int i = 0; i < 70; ++i) j.days.push_back({ i, 1, 0 });
    for (int i = 0; i < 8; ++i) j.gains.push_back({ "g", 10 - i, 1.0f });
    for (int i = 0; i < 8; ++i) j.losses.push_back({ "l", -10 + i, 1.0f });
    j.history.push_back({ std::string(200, 'x'), 1, 1.0f });
    const auto n = Normalize(j);
    CHECK(n.history.size() == kMaxHistory);
    CHECK(n.history[0].label == "h0"); // newest first: the first ones are kept
    CHECK(n.days.size() == kMaxDays);
    CHECK(n.days.front().day == 10);   // oldest first: the newest sixty are kept
    CHECK(n.days.back().day == 69);
    CHECK(n.gains.size() == kMaxMoments);
    CHECK(n.gains.back().points == 6);
    CHECK(n.losses.size() == kMaxMoments);
    CHECK(n.losses.back().points == -6);

    Journal longLabel;
    longLabel.history.push_back({ std::string(200, 'x'), 1, 1.0f });
    CHECK(Normalize(longLabel).history[0].label.size() == kMaxLabelBytes);
}

TEST(JournalNormalize_SecondBeganDropped)
{
    Journal j;
    j.milestones = { { MilestoneKind::Began, 1, 1.0f }, { MilestoneKind::Reached, 2, 2.0f },
        { MilestoneKind::Began, 3, 3.0f } };
    const auto n = Normalize(j);
    CHECK(Count(n, MilestoneKind::Began) == 1);
    CHECK(n.milestones.size() == 2);
    CHECK(n.milestones[0] == (Milestone{ MilestoneKind::Began, 1, 1.0f }));
}

TEST(JournalNormalize_BeganMovedFirst)
{
    Journal j;
    j.milestones = { { MilestoneKind::Reached, 2, 2.0f }, { MilestoneKind::Fell, 1, 3.0f },
        { MilestoneKind::Began, 1, 1.0f } };
    const auto n = Normalize(j);
    CHECK(n.milestones.size() == 3);
    CHECK(n.milestones[0].kind == MilestoneKind::Began);
    CHECK(n.milestones[1] == (Milestone{ MilestoneKind::Reached, 2, 2.0f }));
    CHECK(n.milestones[2] == (Milestone{ MilestoneKind::Fell, 1, 3.0f }));
}

TEST(JournalNormalize_FellTrimmedKeepingNewest)
{
    Journal j;
    j.milestones.push_back({ MilestoneKind::Began, 1, 0.0f });
    for (int i = 0; i < 25; ++i) j.milestones.push_back({ MilestoneKind::Fell, 1, static_cast<float>(i) });
    const auto n = Normalize(j);
    CHECK(Count(n, MilestoneKind::Fell) == kMaxFell);
    CHECK(n.milestones[1].day == 5.0f);
    CHECK(n.milestones.back().day == 24.0f);
}

TEST(JournalNormalize_ReachedOncePerTier)
{
    // A tier is reached once and only tiers 2 to 6 can be reached, so Record can never outgrow the save's limit.
    Journal j;
    j.milestones = { { MilestoneKind::Began, 1, 0.0f }, { MilestoneKind::Reached, 2, 1.0f },
        { MilestoneKind::Reached, 2, 2.0f }, { MilestoneKind::Reached, 1, 3.0f }, { MilestoneKind::Reached, 0, 3.0f } };
    const auto n = Normalize(j);
    CHECK(n.milestones.size() == 2);
    CHECK(n.milestones[1] == (Milestone{ MilestoneKind::Reached, 2, 1.0f }));
}

TEST(JournalNormalize_MilestoneLevelsClamped)
{
    Journal j;
    j.milestones = { { MilestoneKind::Began, 0, 0.0f }, { MilestoneKind::Fell, 0, 1.0f } };
    const auto n = Normalize(j);
    CHECK(n.milestones[0].level == 1);
    CHECK(n.milestones[1].level == 1);
}

TEST(JournalNormalize_MomentsResorted)
{
    Journal j;
    j.gains = { { "small", 2, 1.0f }, { "big", 9, 2.0f }, { "tie first", 5, 3.0f }, { "tie second", 5, 4.0f } };
    j.losses = { { "small", -2, 1.0f }, { "big", -9, 2.0f } };
    const auto n = Normalize(j);
    CHECK(n.gains[0].label == "big");
    CHECK(n.gains[1].label == "tie first");
    CHECK(n.gains[2].label == "tie second");
    CHECK(n.gains[3].label == "small");
    CHECK(n.losses[0].label == "big");
    CHECK(n.losses[1].label == "small");
}

TEST(JournalNormalize_MomentsOfTheWrongSignDropped)
{
    Journal j;
    j.gains = { { "zero", 0, 1.0f }, { "loss", -3, 1.0f }, { "gain", 3, 1.0f } };
    j.losses = { { "gain", 3, 1.0f }, { "loss", -3, 1.0f } };
    const auto n = Normalize(j);
    CHECK(n.gains.size() == 1);
    CHECK(n.gains[0].label == "gain");
    CHECK(n.losses.size() == 1);
    CHECK(n.losses[0].label == "loss");
}

TEST(JournalNormalize_DaysCleaned)
{
    // A negative or non-finite day becomes what Record stores for it: 0, shown as Day 1.
    Journal j;
    j.history = { { "nan", 1, kNan }, { "neg", 2, -4.0f } };
    j.gains = { { "inf", 3, kInf } };
    j.losses = { { "neg inf", -3, -kInf } };
    j.milestones = { { MilestoneKind::Began, 1, kNan }, { MilestoneKind::Fell, 1, -2.0f } };
    j.days = { { -3, 1, 0 }, { 0, 2, 0 }, { 4, 0, 1 } };
    const auto n = Normalize(j);
    for (const auto* list : { &n.history, &n.gains, &n.losses }) {
        for (const auto& c : *list) CHECK(c.day == 0.0f);
    }
    for (const auto& m : n.milestones) CHECK(m.day == 0.0f);
    CHECK(n.days.size() == 2);
    CHECK(n.days[0] == (Day{ 0, 3, 0 }));
    CHECK(n.days[1] == (Day{ 4, 0, 1 }));
    CHECK(DayNumber(n.history[0].day) == 1);
}

// RecordReset: a reset is a fresh start, not a wound.

TEST(JournalReset_LoverResetIsAFreshStart)
{
    Journal j;
    Begin(j, 1, 1.0f);
    Record(j, "climb", 2000, 1, 5, 2.0f);
    Record(j, "small loss", -10, 5, 5, 3.0f);
    const auto before = j;
    RecordReset(j, "Bond reset", 1990, 5, 4.0f);
    CHECK(Count(j, MilestoneKind::Began) == 1);
    CHECK(j.milestones.front() == before.milestones.front());
    CHECK(Count(before, MilestoneKind::Reached) == 4);
    CHECK(Count(j, MilestoneKind::Reached) == 0);
    CHECK(Count(j, MilestoneKind::Fell) == 1);
    CHECK(j.milestones.back() == (Milestone{ MilestoneKind::Fell, 1, 4.0f }));
    CHECK(j.history.size() == before.history.size() + 1);
    CHECK(j.history[0] == (Change{ "Bond reset", -1990, 4.0f }));
    CHECK(j.gains == before.gains);
    CHECK(j.losses == before.losses);
    CHECK(j.days == before.days);

    // A later climb writes Reached again.
    Record(j, "again", 600, 1, 2, 5.0f);
    CHECK(Has(j, MilestoneKind::Reached, 2));
    CHECK(Count(j, MilestoneKind::Reached) == 1);
}

TEST(JournalReset_AtFirstTierWritesNoFell)
{
    Journal j;
    Record(j, "a little", 300, 1, 1, 1.0f);
    RecordReset(j, "Bond reset", 300, 1, 2.0f);
    CHECK(j.history.size() == 2);
    CHECK(j.history[0].points == -300);
    CHECK(Count(j, MilestoneKind::Fell) == 0);
    CHECK(Count(j, MilestoneKind::Began) == 1);
}

TEST(JournalReset_NothingHeldWritesNothing)
{
    Journal j;
    RecordReset(j, "Bond reset", 0, 5, 2.0f);
    RecordReset(j, "Bond reset", -40, 5, 2.0f);
    CHECK(j == Journal{});
}

TEST(JournalReset_BeginsAnUnbegunJournal)
{
    Journal j;
    RecordReset(j, "Bond reset", 1200, 3, 2.0f);
    CHECK(j.milestones.front().kind == MilestoneKind::Began);
    CHECK(Count(j, MilestoneKind::Reached) == 0);
    CHECK(Count(j, MilestoneKind::Fell) == 1);
}

TEST(JournalReset_FullFellListStaysAtItsLimit)
{
    Journal j;
    Record(j, "start", 1500, 1, 4, 1.0f);
    for (int i = 0; i < 25; ++i) {
        Record(j, "fall", -10, 4, 3, 10.0f + static_cast<float>(i));
        Record(j, "rise", 10, 3, 4, 10.5f + static_cast<float>(i));
    }
    for (int i = 0; i < 40; ++i) RecordReset(j, std::string(300, 'r'), 1500, 4, 50.0f + static_cast<float>(i));
    CHECK(Count(j, MilestoneKind::Fell) == kMaxFell);
    CHECK(j.milestones.size() <= 26);
    CHECK(j.history.size() == kMaxHistory);
    CHECK(j.history[0].label.size() == kMaxLabelBytes);
    CHECK(j.milestones.back().day == 89.0f);
}

TEST(JournalReset_NormalizeLeavesItUnchanged)
{
    Journal j;
    Record(j, "climb", 2600, 1, 6, 1.0f);
    Record(j, "slip", -50, 6, 6, 2.0f);
    RecordReset(j, "Bond reset", 2550, 6, 3.0f);
    CHECK(Normalize(j) == j);
    Record(j, "again", 700, 1, 2, 4.0f);
    CHECK(Normalize(j) == j);
}
