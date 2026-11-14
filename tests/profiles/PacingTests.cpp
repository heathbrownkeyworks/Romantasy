#include "check.h"
#include "romance/Pacing.h"

#include <cstdint>
#include <limits>

using namespace romantasy::pacing;

namespace
{
    constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
    constexpr std::int32_t kMin = std::numeric_limits<std::int32_t>::min();
    constexpr float kNan = std::numeric_limits<float>::quiet_NaN();
    constexpr float kInf = std::numeric_limits<float>::infinity();
}

TEST(Pacing_ParseAndKey)
{
    for (Pace p : { Pace::Slow, Pace::Standard, Pace::Fast }) {
        CHECK(ParsePace(Key(p)) == p);
    }
    for (TierMoment m : { TierMoment::Calm, TierMoment::Immediate }) {
        CHECK(ParseTierMoment(Key(m)) == m);
    }
    CHECK(Key(Pace::Slow) == "slow");
    CHECK(Key(Pace::Standard) == "standard");
    CHECK(Key(Pace::Fast) == "fast");
    CHECK(Key(TierMoment::Calm) == "calm");
    CHECK(Key(TierMoment::Immediate) == "immediate");
    CHECK(!ParsePace("Fast"));
    CHECK(!ParsePace(""));
    CHECK(!ParsePace("turbo"));
    CHECK(!ParseTierMoment("Calm"));
    CHECK(!ParseTierMoment(""));
    CHECK(!ParseTierMoment("turbo"));
}

TEST(Pacing_FastDoublesGainsOnly)
{
    CHECK(ApplyPace(Pace::Fast, 3, false).points == 6);
    CHECK(ApplyPace(Pace::Fast, -3, false).points == -3);
    CHECK(ApplyPace(Pace::Fast, 0, false).points == 0);
    CHECK(ApplyPace(Pace::Fast, 3, true).carry == true);
    CHECK(ApplyPace(Pace::Fast, 3, false).carry == false);
    CHECK(ApplyPace(Pace::Fast, kMax, false).points == kMax);
    CHECK(ApplyPace(Pace::Fast, kMax / 2 + 1, false).points == kMax);
}

TEST(Pacing_SlowCarriesHalves)
{
    Paced a = ApplyPace(Pace::Slow, 1, false);
    CHECK(a.points == 0);
    CHECK(a.carry == true);
    Paced b = ApplyPace(Pace::Slow, 1, a.carry);
    CHECK(b.points == 1);
    CHECK(b.carry == false);

    Paced c = ApplyPace(Pace::Slow, 3, false);
    CHECK(c.points == 1);
    CHECK(c.carry == true);
    Paced d = ApplyPace(Pace::Slow, 3, true);
    CHECK(d.points == 2);
    CHECK(d.carry == false);

    Paced loss = ApplyPace(Pace::Slow, -1, true);
    CHECK(loss.points == -1);
    CHECK(loss.carry == true);

    Paced big = ApplyPace(Pace::Slow, kMax, true);
    CHECK(big.points == kMax / 2 + 1);
    CHECK(big.carry == false);
    CHECK(ApplyPace(Pace::Slow, kMin, true).points == kMin);
}

TEST(Pacing_StandardUnchanged)
{
    CHECK(ApplyPace(Pace::Standard, 5, false).points == 5);
    CHECK(ApplyPace(Pace::Standard, 5, true).carry == true);
    CHECK(ApplyPace(Pace::Standard, -5, false).points == -5);
    CHECK(ApplyPace(Pace::Standard, 0, false).points == 0);
    CHECK(ApplyPace(Pace::Standard, kMax, false).points == kMax);
    CHECK(ApplyPace(Pace::Standard, kMin, false).points == kMin);
}

TEST(Pacing_DaysNeeded)
{
    CHECK(DaysNeeded(1) == 0.0f);
    CHECK(DaysNeeded(2) == 0.0f);
    CHECK(DaysNeeded(3) == 3.0f);
    CHECK(DaysNeeded(4) == 7.0f);
    CHECK(DaysNeeded(5) == 14.0f);
    CHECK(DaysNeeded(6) == 21.0f);
    CHECK(DaysNeeded(0) == 0.0f);
    CHECK(DaysNeeded(7) == 0.0f);
    CHECK(DaysNeeded(-1) == 0.0f);
}

TEST(Pacing_GateCap)
{
    CHECK(GateCap(0.0f) == 999);
    CHECK(GateCap(2.9f) == 999);
    CHECK(GateCap(3.0f) == 1499);
    CHECK(GateCap(7.0f) == 1999);
    CHECK(GateCap(14.0f) == 2499);
    CHECK(GateCap(21.0f) == kMax);
    // Not a number, infinite or negative counts as no days together.
    CHECK(GateCap(kNan) == 999);
    CHECK(GateCap(kInf) == 999);
    CHECK(GateCap(-kInf) == 999);
    CHECK(GateCap(-5.0f) == 999);
}

TEST(Pacing_GatedPoints)
{
    CHECK(GatedPoints(990, 1010, 1.0f) == 999);
    CHECK(GatedPoints(999, 1004, 1.0f) == 999);
    CHECK(GatedPoints(1200, 1210, 1.0f) == 1210);
    CHECK(GatedPoints(1200, 1100, 1.0f) == 1100);
    CHECK(GatedPoints(990, 1010, 3.0f) == 1010);
    CHECK(GatedPoints(400, 520, 0.0f) == 520);
    CHECK(GatedPoints(100, 100, 0.0f) == 100);
    CHECK(GatedPoints(990, 1010, kNan) == 999);
    CHECK(GatedPoints(990, 1010, -2.0f) == 999);
    CHECK(GatedPoints(0, kMax, 0.0f) == 999);
    CHECK(GatedPoints(kMax - 1, kMax, 0.0f) == kMax);
    CHECK(GatedPoints(0, kMax, 21.0f) == kMax);
    CHECK(GatedPoints(kMax, kMin, 0.0f) == kMin);
}

TEST(Pacing_NextGate)
{
    auto g = NextGate(999, 1.0f);
    CHECK(g.has_value());
    CHECK(g && g->level == 3);
    CHECK(g && g->daysNeeded == 3.0f);
    CHECK(!NextGate(999, 3.0f));
    CHECK(!NextGate(300, 0.0f));
    auto s = NextGate(2499, 20.0f);
    CHECK(s.has_value());
    CHECK(s && s->level == 6);
    CHECK(s && s->daysNeeded == 21.0f);
    CHECK(!NextGate(2600, 0.0f));
    CHECK(!NextGate(kMax, 0.0f));
    CHECK(NextGate(999, kNan).has_value());
    CHECK(NextGate(-50, 0.0f) == std::nullopt);
}

TEST(Pacing_CalmAndAnnounce)
{
    CHECK(IsCalm({ false, true, false }));
    CHECK(!IsCalm({ true, true, false }));
    CHECK(IsCalm({ false, false, true }));
    CHECK(!IsCalm({ false, false, false }));

    const CalmFacts wild{ false, false, false };
    const CalmFacts fight{ true, false, false };
    const CalmFacts inn{ false, true, false };

    CHECK(ShouldAnnounce(TierMoment::Immediate, wild, 0.0f));
    CHECK(ShouldAnnounce(TierMoment::Immediate, inn, 0.0f));
    CHECK(!ShouldAnnounce(TierMoment::Immediate, fight, 9.0f));

    CHECK(!ShouldAnnounce(TierMoment::Calm, wild, 0.5f));
    CHECK(ShouldAnnounce(TierMoment::Calm, wild, 1.0f));
    CHECK(!ShouldAnnounce(TierMoment::Calm, fight, 5.0f));
    CHECK(ShouldAnnounce(TierMoment::Calm, inn, 0.0f));
    CHECK(!ShouldAnnounce(TierMoment::Calm, wild, kNan));
    CHECK(!ShouldAnnounce(TierMoment::Calm, wild, -3.0f));
    CHECK(!ShouldAnnounce(TierMoment::Calm, wild, kInf));
}

TEST(Calm_SceneHoldsEverything)
{
    const CalmFacts scene{ .calmPlace = true, .justRested = true, .inScene = true };
    CHECK(!IsCalm(scene));
    CHECK(!ShouldAnnounce(TierMoment::Immediate, scene, 0.0f));
    CHECK(!ShouldAnnounce(TierMoment::Immediate, scene, 5.0f));
    CHECK(!ShouldAnnounce(TierMoment::Calm, scene, 0.0f));
    CHECK(!ShouldAnnounce(TierMoment::Calm, scene, 5.0f));

    // The same facts out of a scene keep today's answers.
    const CalmFacts calm{ .calmPlace = true, .justRested = true };
    CHECK(IsCalm(calm));
    CHECK(ShouldAnnounce(TierMoment::Immediate, calm, 0.0f));
    CHECK(ShouldAnnounce(TierMoment::Calm, calm, 0.0f));
    const CalmFacts wild{};
    CHECK(ShouldAnnounce(TierMoment::Calm, wild, 5.0f));
    CHECK(!ShouldAnnounce(TierMoment::Calm, CalmFacts{ .inScene = true }, 5.0f));
}

TEST(Pacing_Merge)
{
    const auto first = Merge(std::nullopt, { 2, 3, 5, 4.0f });
    CHECK(first == std::optional<PendingChange>(PendingChange{ 2, 3, 5, 4.0f }));

    const PendingChange waiting{ 2, 3, 12, 4.0f };
    const auto joined = Merge(waiting, { 3, 4, 8, 5.0f });
    CHECK(joined == std::optional<PendingChange>(PendingChange{ 2, 4, 20, 4.0f }));

    CHECK(!Merge(waiting, { 3, 2, -6, 5.0f }));

    const auto down = Merge(PendingChange{ 3, 2, -10, 1.0f }, { 2, 1, -10, 2.0f });
    CHECK(down == std::optional<PendingChange>(PendingChange{ 3, 1, -20, 1.0f }));

    CHECK(!Merge(std::nullopt, { 2, 2, 7, 1.0f }));

    const auto huge = Merge(PendingChange{ 1, 2, kMax, 0.0f }, { 2, 3, kMax, 1.0f });
    CHECK(huge.has_value());
    CHECK(huge && huge->pointsDelta == kMax);
    const auto tiny = Merge(PendingChange{ 3, 2, kMin, 0.0f }, { 2, 1, kMin, 1.0f });
    CHECK(tiny && tiny->pointsDelta == kMin);
}

TEST(Pacing_MergeKeepsTheWhisper)
{
    // A change made by a developer tool does not whisper; merged with a real one, the merged change does.
    PendingChange quiet{ 2, 3, 10, 1.0f };
    quiet.whisper = false;
    PendingChange real{ 3, 4, 10, 2.0f };
    CHECK(Merge(quiet, real)->whisper);
    PendingChange quietLater{ 4, 5, 10, 1.0f };
    quietLater.whisper = false;
    CHECK(Merge(real, quietLater)->whisper);
    CHECK(!Merge(real, quiet));  // back at the old tier: nothing to merge
    PendingChange quietToo{ 3, 4, 10, 2.0f };
    quietToo.whisper = false;
    CHECK(!Merge(quiet, quietToo)->whisper);
    CHECK(!Merge(std::nullopt, quiet)->whisper);
    // The release keeps it: announced for the level held now, with the flag it was held with.
    const auto released = DecideRelease(quiet, { .followerKnown = true, .currentLevel = 4,
        .moment = TierMoment::Immediate });
    CHECK(released.what == Release::Announce && !released.change.whisper && released.change.newLevel == 4);
    CHECK(PendingChange{}.whisper);  // read back from a save
}

TEST(Pacing_CalmPlaceKeywords)
{
    // Inn, player house, house, dwelling, city, town, settlement, in Skyrim.esm.
    constexpr std::array<std::uint32_t, 7> expected{ 0x1CB87, 0xFC1A3, 0x1CB85, 0x130DC, 0x13168, 0x13166, 0x13167 };
    CHECK(kCalmPlaceKeywords == expected);
}

TEST(Pacing_GateNeverFreezesTheTierHeld)
{
    CHECK(GatedPoints(1200, 1300, 1.0f) == 1300);
    CHECK(GatedPoints(1450, 1550, 1.0f) == 1499);
    CHECK(GatedPoints(1450, 1550, 7.0f) == 1550);
    CHECK(GatedPoints(400, 600, 0.0f) == 600);
    CHECK(GatedPoints(900, 1100, 0.0f) == 999);
    CHECK(GatedPoints(1200, 1000, 0.0f) == 1000);
    CHECK(GatedPoints(1200, 0, 0.0f) == 0);
    CHECK(GatedPoints(2500, 9000, 0.0f) == 9000);
    CHECK(GatedPoints(2600, kMax, 0.0f) == kMax);
    CHECK(TierTop(1200) == 1499);
    CHECK(TierTop(0) == 499);
    CHECK(TierTop(-5) == 499);
    CHECK(TierTop(2500) == kMax);
    // The gate really ahead of Friend at 1200 with 1 day is Confidant's 7 days.
    const auto ahead = NextGate(1200, 1.0f);
    CHECK(ahead && ahead->level == 4 && ahead->daysNeeded == 7.0f);
}

TEST(Pacing_GateGain)
{
    // Standard, Fast and the developer tools never gate.
    for (Pace p : { Pace::Standard, Pace::Fast }) {
        const auto r = GateGain(900, 200, 0.0f, p, true);
        CHECK(r.newPoints == 1100 && r.applied == 200 && !r.refused);
    }
    const auto tool = GateGain(900, 200, 0.0f, Pace::Slow, false);
    CHECK(tool.newPoints == 1100 && tool.applied == 200 && !tool.refused);
    // Partly held, fully refused, inside the tier held.
    const auto part = GateGain(900, 200, 0.0f, Pace::Slow, true);
    CHECK(part.newPoints == 999 && part.applied == 99 && !part.refused);
    const auto none = GateGain(999, 5, 0.0f, Pace::Slow, true);
    CHECK(none.newPoints == 999 && none.applied == 0 && none.refused);
    const auto inside = GateGain(1200, 100, 1.0f, Pace::Slow, true);
    CHECK(inside.newPoints == 1300 && inside.applied == 100 && !inside.refused);
    // A loss passes through, before the clamp at 0.
    const auto loss = GateGain(100, -300, 0.0f, Pace::Slow, true);
    CHECK(loss.newPoints == -200 && loss.applied == -300 && !loss.refused);
    // int32 limits saturate instead of overflowing.
    const auto high = GateGain(kMax - 10, 100, 0.0f, Pace::Fast, true);
    CHECK(high.newPoints == kMax && high.applied == 10 && !high.refused);
    const auto low = GateGain(kMin + 10, -100, 0.0f, Pace::Standard, true);
    CHECK(low.newPoints == kMin && low.applied == -10 && !low.refused);
    const auto slowHigh = GateGain(kMax - 10, kMax, 30.0f, Pace::Slow, true);
    CHECK(slowHigh.newPoints == kMax && slowHigh.applied == 10 && !slowHigh.refused);
    const auto wide = GateGain(kMin, kMax, 0.0f, Pace::Standard, true);
    CHECK(wide.newPoints == -1 && wide.applied == kMax);
}

TEST(Pacing_ReleaseTarget)
{
    const PendingChange waiting{ 2, 3, 40, 5.0f };
    CHECK(ReleaseTarget(waiting, 3) == waiting);
    CHECK(!ReleaseTarget(waiting, 2));
    const auto higher = ReleaseTarget(waiting, 4);
    CHECK(higher && higher->oldLevel == 2 && higher->newLevel == 4 && higher->pointsDelta == 40 && higher->since == 5.0f);
    const auto fell = ReleaseTarget(waiting, 1);
    CHECK(fell && fell->oldLevel == 2 && fell->newLevel == 1);
    CHECK(!ReleaseTarget(waiting, 0));
    CHECK(!ReleaseTarget(waiting, 7));
}

TEST(Pacing_DecideRelease)
{
    const PendingChange waiting{ 2, 3, 40, 5.0f };
    const CalmFacts quiet{ false, false, false };
    const CalmFacts fighting{ true, false, false };
    const CalmFacts inn{ false, true, false };
    const auto facts = [&](bool known, bool enabled, std::int32_t level, TierMoment moment, CalmFacts calm, float waited) {
        return ReleaseFacts{ known, enabled, level, moment, calm, waited };
    };

    // Calm switched to At once: ShouldAnnounce(Immediate) is just "out of combat".
    CHECK(ShouldAnnounce(TierMoment::Immediate, quiet, 0.0f));
    CHECK(!ShouldAnnounce(TierMoment::Immediate, fighting, 5.0f));
    const auto atOnce = DecideRelease(waiting, facts(true, true, 3, TierMoment::Immediate, quiet, 0.0f));
    CHECK(atOnce.what == Release::Announce && atOnce.change == waiting);
    CHECK(DecideRelease(waiting, facts(true, true, 3, TierMoment::Immediate, fighting, 0.0f)).what == Release::Keep);

    // The one-day limit, out of and in combat.
    CHECK(DecideRelease(waiting, facts(true, true, 3, TierMoment::Calm, quiet, 1.0f)).what == Release::Announce);
    CHECK(DecideRelease(waiting, facts(true, true, 3, TierMoment::Calm, quiet, 0.9f)).what == Release::Keep);
    CHECK(DecideRelease(waiting, facts(true, true, 3, TierMoment::Calm, fighting, 3.0f)).what == Release::Keep);

    // Back at the old level: kept while it may not announce, dropped once it may.
    CHECK(DecideRelease(waiting, facts(true, true, 2, TierMoment::Calm, fighting, 3.0f)).what == Release::Keep);
    CHECK(DecideRelease(waiting, facts(true, true, 2, TierMoment::Calm, inn, 0.0f)).what == Release::Drop);

    // A further move is announced for the level held.
    const auto further = DecideRelease(waiting, facts(true, true, 4, TierMoment::Calm, inn, 0.0f));
    CHECK(further.what == Release::Announce && further.change.oldLevel == 2 && further.change.newLevel == 4);

    // Unknown follower keeps; a switched-off bond drops, even mid-fight or back at its old level.
    CHECK(DecideRelease(waiting, facts(false, true, 3, TierMoment::Calm, inn, 9.0f)).what == Release::Keep);
    CHECK(DecideRelease(waiting, facts(false, false, 3, TierMoment::Calm, inn, 9.0f)).what == Release::Keep);
    CHECK(DecideRelease(waiting, facts(true, false, 3, TierMoment::Calm, fighting, 0.0f)).what == Release::Drop);

    // A waited time of 0: only a calm moment announces.
    CHECK(DecideRelease(waiting, facts(true, true, 3, TierMoment::Calm, inn, 0.0f)).what == Release::Announce);
    CHECK(DecideRelease(waiting, facts(true, true, 3, TierMoment::Calm, quiet, 0.0f)).what == Release::Keep);

    // Parked one-liners.
    CHECK(ShouldAnnounce(TierMoment::Calm, { false, false, true }, 0.0f));
    const auto merged = Merge(PendingChange{ 2, 3, 10, 5.0f }, { 3, 4, 20, 2.0f });
    CHECK(merged && merged->since == 5.0f && merged->oldLevel == 2 && merged->newLevel == 4);
}

TEST(Pacing_AwardForStat)
{
    // Gains under each pace.
    const auto standard = AwardForStat(100, 10, Pace::Standard, false, 0.0f, true);
    CHECK(standard.applied == 10 && standard.reported == 10 && !standard.carry && !standard.refused);
    const auto fast = AwardForStat(100, 10, Pace::Fast, false, 0.0f, true);
    CHECK(fast.applied == 20 && fast.reported == 20 && !fast.refused);
    const auto slow = AwardForStat(100, 10, Pace::Slow, false, 0.0f, true);
    CHECK(slow.applied == 5 && slow.reported == 5 && !slow.carry);

    // The carry bit across two calls.
    const auto first = AwardForStat(100, 3, Pace::Slow, false, 0.0f, true);
    CHECK(first.applied == 1 && first.reported == 1 && first.carry);
    const auto second = AwardForStat(101, 3, Pace::Slow, first.carry, 0.0f, true);
    CHECK(second.applied == 2 && second.reported == 2 && !second.carry);

    // A loss under each pace is what was asked, carry untouched.
    for (Pace p : { Pace::Slow, Pace::Standard, Pace::Fast }) {
        const auto loss = AwardForStat(100, -10, p, true, 0.0f, true);
        CHECK(loss.applied == -10 && loss.reported == -10 && loss.carry && !loss.refused);
    }

    // Partly held: what was awarded.
    const auto part = AwardForStat(900, 400, Pace::Slow, false, 0.0f, true);
    CHECK(part.applied == 99 && part.reported == 99 && !part.refused);

    // Refused in full: nothing applied, the deed's paced worth reported, the carry it came in with kept.
    const auto refused = AwardForStat(999, 9, Pace::Slow, true, 0.0f, true);
    CHECK(refused.applied == 0 && refused.reported == 5 && refused.carry && refused.refused);

    // Paced to 0: nothing reported, the half point carried.
    const auto rounded = AwardForStat(100, 1, Pace::Slow, false, 0.0f, true);
    CHECK(rounded.applied == 0 && rounded.reported == 0 && rounded.carry && !rounded.refused);

    // Developer tools are paced but never gated.
    const auto tool = AwardForStat(900, 400, Pace::Slow, false, 0.0f, false);
    CHECK(tool.applied == 200 && tool.reported == 200 && !tool.refused);
}
