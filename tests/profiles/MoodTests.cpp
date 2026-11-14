#include "check.h"
#include "romance/Mood.h"

#include <cstdint>
#include <cstdlib>
#include <limits>

using namespace romantasy::mood;

TEST(Mood_GainAndLossRules)
{
    CHECK(AfterPoints({}, 3, 0.0f).score == 12);
    CHECK(AfterPoints({}, 10, 0.0f).score == 20);
    CHECK(AfterPoints({}, -2, 0.0f).score == -12);
    CHECK(AfterPoints({}, -10, 0.0f).score == -30);

    Mood up;
    for (int i = 0; i < 20; ++i) up = AfterPoints(up, 10, 0.0f);
    CHECK(up.score == 100);
    Mood down;
    for (int i = 0; i < 20; ++i) down = AfterPoints(down, -10, 0.0f);
    CHECK(down.score == -100);
}

TEST(Mood_DriftsTowardZero)
{
    const Mood warm{ 60, 10.0f };
    CHECK(Drifted(warm, 11.0f) == 35);
    CHECK(Drifted(warm, 12.0f) == 10);
    CHECK(Drifted(warm, 13.0f) == 0);
    CHECK(Drifted(warm, 20.0f) == 0);

    const Mood cold{ -40, 10.0f };
    CHECK(Drifted(cold, 11.0f) == -15);
    CHECK(Drifted(cold, 12.0f) == 0);

    // Half a day is 12.5 points; the drift is truncated, so 12 points move.
    CHECK(Drifted(warm, 10.5f) == 48);
    CHECK(Drifted(cold, 10.5f) == -28);

    CHECK(Drifted(warm, 9.0f) == 60);
}

TEST(Mood_States)
{
    CHECK(StateOf(25) == State::Warm);
    CHECK(StateOf(24) == State::Settled);
    CHECK(StateOf(-24) == State::Settled);
    CHECK(StateOf(-25) == State::Strained);
    CHECK(Name(State::Strained) == "Strained");
    CHECK(Name(State::Settled) == "Settled");
    CHECK(Name(State::Warm) == "Warm");
    CHECK(Key(State::Strained) == "strained");
    CHECK(Key(State::Settled) == "settled");
    CHECK(Key(State::Warm) == "warm");
    CHECK(static_cast<int>(State::Strained) == 1);
    CHECK(static_cast<int>(State::Settled) == 2);
    CHECK(static_cast<int>(State::Warm) == 3);
    CHECK(StateAt({ 50, 0.0f }, 1.0f) == State::Warm);
    CHECK(StateAt({ 50, 0.0f }, 2.0f) == State::Settled);
}

TEST(Mood_AfterPointsDriftsFirst)
{
    const Mood result = AfterPoints({ 50, 0.0f }, 1, 2.0f);
    CHECK(result.score == 4);
    CHECK(result.at == 2.0f);
}

TEST(Mood_Nudges)
{
    CHECK(Nudged({}, kReunionNudge, 1.0f).score == 15);
    CHECK(Nudged({}, kReunionNudge, 1.0f).at == 1.0f);
    CHECK(Nudged({}, kTierLostNudge, 1.0f).score == -20);
    CHECK(Nudged({ 100, 1.0f }, kReunionNudge, 1.0f).score == 100);
    CHECK(Nudged({ -100, 1.0f }, kTierLostNudge, 1.0f).score == -100);
}

TEST(Company_FirstSightNotFollowingIsNotAway)
{
    Company company;
    for (float day : { 0.0f, 1.0f, 5.0f, 40.0f }) {
        const CompanyStep step = Observe(company, false, day);
        CHECK(step.event == CompanyEvent::None);
        CHECK(step.next == Company{});
        company = step.next;
    }
}

TEST(Company_ReunionNeedsADay)
{
    Company c = Observe({}, true, 4.0f).next;
    c = Observe(c, false, 5.0f).next;
    CHECK(c.away);

    const CompanyStep early = Observe(c, true, 5.5f);
    CHECK(early.event == CompanyEvent::None);
    CHECK(!early.next.away);

    const CompanyStep full = Observe(c, true, 6.0f);
    CHECK(full.event == CompanyEvent::Reunion);
    CHECK(full.daysApart == 1.0f);
    CHECK(!full.next.away);
    CHECK(!full.next.apartSent);
}

TEST(Company_TimeApartOnce)
{
    Company c = Observe({}, true, 4.0f).next;
    c = Observe(c, false, 5.0f).next;
    CHECK(c.awaySince == 5.0f);

    CHECK(Observe(c, false, 7.9f).event == CompanyEvent::None);

    const CompanyStep sent = Observe(c, false, 8.0f);
    CHECK(sent.event == CompanyEvent::TimeApart);
    CHECK(sent.daysApart == 3.0f);
    c = sent.next;
    CHECK(c.apartSent);

    CHECK(Observe(c, false, 9.0f).event == CompanyEvent::None);
    CHECK(Observe(c, false, 30.0f).event == CompanyEvent::None);

    const CompanyStep back = Observe(c, true, 31.0f);
    CHECK(back.event == CompanyEvent::Reunion);
    CHECK(back.daysApart == 26.0f);
    CHECK(!back.next.apartSent);

    Company again = Observe(back.next, false, 32.0f).next;
    CHECK(Observe(again, false, 35.0f).event == CompanyEvent::TimeApart);
}

TEST(Company_DaysTogether)
{
    Company c = Observe({}, true, 2.0f).next;
    c = Observe(c, true, 2.5f).next;
    c = Observe(c, true, 4.0f).next;
    CHECK(c.daysTogether == 2.0f);

    c = Observe(c, false, 4.0f).next;
    c = Observe(c, true, 9.0f).next;
    c = Observe(c, true, 10.0f).next;
    CHECK(c.daysTogether == 3.0f);

    c = Observe(c, true, 8.0f).next;
    CHECK(c.daysTogether == 3.0f);
    CHECK(c.lastSeen == 8.0f);
}

TEST(Company_ClockBackwards)
{
    Company c = Observe({}, true, 1.0f).next;
    c = Observe(c, false, 10.0f).next;
    const CompanyStep step = Observe(c, true, 4.0f);
    CHECK(step.event == CompanyEvent::None);
    CHECK(!step.next.away);
    CHECK(step.daysApart == 0.0f);
}

TEST(Mood_ManySmallStepsDriftLikeOneLarge)
{
    const Mood start{ 60, 0.0f };
    const Mood once = AfterPoints(start, 0, 1.0f);
    CHECK(once.score == 35);

    Mood m = start;
    for (int i = 1; i <= 100; ++i) m = AfterPoints(m, 0, static_cast<float>(i) * 0.01f);
    CHECK(std::abs(m.score - once.score) <= 1);
}

TEST(Mood_FrequentGainsStillDrift)
{
    // Two days of +1 every 0.02 days saturates at the cap either way (each gain
    // of 4 beats the 0.5 of drift between gains), so only check the cap there.
    Mood spaced;
    for (int i = 1; i <= 100; ++i) {
        spaced = AfterPoints(spaced, 1, static_cast<float>(i) * 0.02f);
        CHECK(spaced.score <= 100);
    }
    // Before saturation the drift shows: 20 gains over 0.4 days.
    Mood slow;
    Mood instant;
    for (int i = 1; i <= 20; ++i) {
        slow = AfterPoints(slow, 1, static_cast<float>(i) * 0.02f);
        instant = AfterPoints(instant, 1, 0.0f);
    }
    CHECK(instant.score == 80);
    CHECK(slow.score < instant.score);
}

TEST(Mood_NonFiniteNowChangesNothing)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const Mood m{ 40, 3.0f };
    for (float now : { nan, inf }) {
        CHECK(AfterPoints(m, 5, now) == m);
        CHECK(Nudged(m, 15, now) == m);
        const Company c = Observe({}, true, 1.0f).next;
        const CompanyStep step = Observe(c, false, now);
        CHECK(step.next == c);
        CHECK(step.event == CompanyEvent::None);
    }
}

TEST(Mood_BackwardsClock)
{
    const Mood m{ 40, 10.0f };
    const Mood after = AfterPoints(m, 2, 4.0f);
    CHECK(after.score == 48);
    CHECK(after.at == 10.0f);
    CHECK(Nudged(m, -20, 4.0f).score == 20);
    CHECK(Nudged(m, -20, 4.0f).at == 10.0f);
    CHECK(AfterPoints({ 0, 10.0f }, 1, 4.0f).at == 4.0f);
}

TEST(Mood_ExtremePoints)
{
    CHECK(AfterPoints({}, std::numeric_limits<std::int32_t>::max(), 0.0f).score == 20);
    CHECK(AfterPoints({}, std::numeric_limits<std::int32_t>::min(), 0.0f).score == -30);
}

TEST(Company_ReseedOnLoad)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float today = 10.0f;

    // Seen following, lastSeen lost to 0: the next look adds nothing.
    Company together;
    together.seenFollowing = true;
    together.lastSeen = 0.0f;
    together.daysTogether = 4.0f;
    const Reseeded a = Reseed({ 30, -2.0f }, together, today);
    CHECK(a.changed);
    CHECK(a.mood.at == today);
    CHECK(a.mood.score == 30);
    CHECK(a.company.lastSeen == today);
    const CompanyStep afterA = Observe(a.company, true, today);
    CHECK(afterA.event == CompanyEvent::None);
    CHECK(afterA.next.daysTogether == 4.0f);

    // Away since a day later than today, and a NaN mood stamp.
    Company away;
    away.seenFollowing = true;
    away.lastSeen = 3.0f;
    away.away = true;
    away.awaySince = 50.0f;
    const Reseeded b = Reseed({ -40, nan }, away, today);
    CHECK(b.changed);
    CHECK(b.mood.at == today);
    CHECK(b.company.awaySince == today);
    CHECK(b.company.lastSeen == 3.0f);
    CHECK(Observe(b.company, false, today).event == CompanyEvent::None);
    CHECK(Observe(b.company, true, today).event == CompanyEvent::None);

    // A negative and a NaN company stamp; bad days together.
    Company broken;
    broken.seenFollowing = true;
    broken.lastSeen = nan;
    broken.away = true;
    broken.awaySince = -1.0f;
    broken.daysTogether = nan;
    const Reseeded c = Reseed({}, broken, today);
    CHECK(c.changed);
    CHECK(c.company.lastSeen == today);
    CHECK(c.company.awaySince == today);
    CHECK(c.company.daysTogether == 0.0f);
    broken.daysTogether = -3.0f;
    CHECK(Reseed({}, broken, today).company.daysTogether == 0.0f);

    // A sane entry comes back unchanged.
    Company sane;
    sane.seenFollowing = true;
    sane.lastSeen = 8.0f;
    sane.daysTogether = 2.0f;
    const Mood saneMood{ 12, 9.0f };
    const Reseeded d = Reseed(saneMood, sane, today);
    CHECK(!d.changed);
    CHECK(d.mood == saneMood);
    CHECK(d.company == sane);

    // Not away: awaySince is left as it is.
    Company notAway = sane;
    notAway.awaySince = 99.0f;
    const Reseeded e = Reseed(saneMood, notAway, today);
    CHECK(!e.changed);
    CHECK(e.company.awaySince == 99.0f);

    // Never seen following: lastSeen 0 is fine.
    const Reseeded f = Reseed({}, {}, today);
    CHECK(!f.changed);

    // A non-finite today changes nothing.
    const Reseeded g = Reseed({ 5, -1.0f }, broken, nan);
    CHECK(!g.changed);
    CHECK(g.mood.at == -1.0f);
    CHECK(g.company.awaySince == -1.0f);
}

TEST(Company_ReunionWithoutTimeApart)
{
    Company c = Observe({}, true, 1.0f).next;
    c = Observe(c, false, 2.0f).next;
    CHECK(c.away);

    const CompanyStep back = Observe(c, true, 9.0f);
    CHECK(back.event == CompanyEvent::Reunion);
    CHECK(back.daysApart == 7.0f);
    CHECK(!back.next.apartSent);
    CHECK(!back.next.away);

    c = Observe(back.next, false, 10.0f).next;
    CHECK(Observe(c, false, 12.9f).event == CompanyEvent::None);
    const CompanyStep apart = Observe(c, false, 13.0f);
    CHECK(apart.event == CompanyEvent::TimeApart);
    CHECK(apart.daysApart == 3.0f);
    CHECK(Observe(apart.next, false, 14.0f).event == CompanyEvent::None);
}

TEST(Mood_LossThenTierLost)
{
    const Mood start{ 20, 0.0f };
    // One day of drift (25) takes the 20 to 0; -10 points is capped at -30; the nudge adds -20.
    const Mood lost = Nudged(AfterPoints(start, -10, 1.0f), kTierLostNudge, 1.0f);
    CHECK(lost.score == -50);
    CHECK(lost.at == 1.0f);
    CHECK(StateAt(lost, 1.0f) == State::Strained);

    // Strained while |score| >= 25: exactly 25 points of drift later the score is -25.
    const float perDay = static_cast<float>(detail::kDriftPerDay);
    CHECK(Drifted(lost, 1.0f + 25.0f / perDay) == -25);
    CHECK(StateAt(lost, 1.0f + 25.0f / perDay) == State::Strained);
    // One more whole point of drift (26 in all) reads Settled.
    CHECK(Drifted(lost, 1.0f + 26.5f / perDay) == -24);
    CHECK(StateAt(lost, 1.0f + 26.5f / perDay) == State::Settled);
}

TEST(Company_FlappingWithinADay)
{
    Company c;
    const bool pattern[] = { true, true, false, true, true, true };
    for (int i = 0; i < 6; ++i) {
        const CompanyStep step = Observe(c, pattern[i], static_cast<float>(i) * 0.2f);
        CHECK(step.event == CompanyEvent::None);
        c = step.next;
    }
    // Only the spans spent following count: 0.2 + 0.2 + 0.2.
    CHECK_NEAR(c.daysTogether, 0.6f, 0.001f);
}
