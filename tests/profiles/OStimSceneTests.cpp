#include "check.h"
#include "ostim/SceneTracker.h"

#include <cstdint>
#include <vector>

using namespace romantasy::ostim;

namespace
{
    const std::vector<std::uint32_t> kPair{ 0x14, 0xAD005900 };
}

TEST(Scene_NodeBeforeStart)
{
    SceneTracker t;
    CHECK(!t.Node({ "a", Kind::Affection, false }, 1000));
    t.Started({ 0x14 }, 1200);
    CHECK(t.Active());
    const auto end = t.Stopped(kPair, 31000);
    CHECK(end.has_value());
    if (!end) return;
    CHECK_NEAR(end->seconds, 30.0f, 0.001f);
    CHECK(end->participants == kPair);
    CHECK(end->kind == Kind::Affection);
    CHECK(!end->force);
    CHECK(!end->ours);
    CHECK(!end->lost);
    CHECK(!end->stoppedForMismatch);
}

TEST(Scene_DeepestKindAndForce)
{
    SceneTracker t;
    t.Started(kPair, 0);
    CHECK(!t.Node({ "a", Kind::Affection, false }, 100));
    CHECK(!t.Node({ "b", Kind::Intimacy, false }, 200));
    CHECK(!t.Node({ "c", Kind::Affection, false }, 300));
    auto end = t.Stopped(kPair, 1000);
    CHECK(end && end->kind == Kind::Intimacy);
    CHECK(end && !end->force);

    t.Started(kPair, 0);
    CHECK(!t.Node({ "a", Kind::Affection, false }, 100));
    CHECK(!t.Node({ "b", Kind::Affection, true }, 200));
    CHECK(!t.Node({ "c", Kind::Affection, false }, 300));
    end = t.Stopped(kPair, 1000);
    CHECK(end && end->force);
    CHECK(end && end->kind == Kind::Affection);
}

TEST(Scene_StopWithoutRecord)
{
    SceneTracker t;
    CHECK(!t.Active());
    CHECK(!t.Stopped(kPair, 1000).has_value());
    CHECK(!t.Lost(1000).has_value());

    t.Started(kPair, 0);
    CHECK(t.Active());
    CHECK(t.Stopped(kPair, 1000).has_value());
    CHECK(!t.Active());

    t.Started(kPair, 0);
    CHECK(t.Lost(1000).has_value());
    CHECK(!t.Active());

    t.Started(kPair, 0);
    t.Clear();
    CHECK(!t.Active());
    CHECK(!t.Stopped(kPair, 1000).has_value());
}

TEST(Scene_Lost)
{
    SceneTracker t;
    t.Started(kPair, 0);
    CHECK(!t.Node({ "a", Kind::Closeness, false }, 100));
    const auto end = t.Lost(5000);
    CHECK(end.has_value());
    if (!end) return;
    CHECK(end->lost);
    CHECK(end->kind == Kind::Closeness);
    CHECK(end->participants == kPair);
    CHECK(!t.Active());
}

TEST(Scene_OursGentleLeavesKind)
{
    SceneTracker t;
    t.Adopt({ 7, Kind::Affection, "a" });
    CHECK(!t.Node({ "a", Kind::Affection, false }, 0));
    t.Started(kPair, 0);
    CHECK(t.Node({ "b", Kind::Closeness, false }, 1000));
    CHECK(!t.Node({ "c", Kind::Intimacy, false }, 2000));
    const auto end = t.Stopped(kPair, 3000);
    CHECK(end.has_value());
    if (!end) return;
    CHECK(end->ours);
    CHECK(end->oursKind == Kind::Affection);
    CHECK(end->stoppedForMismatch);
}

TEST(Scene_OursWrongFirstNode)
{
    SceneTracker t;
    t.Adopt({ 7, Kind::Closeness, "a" });
    CHECK(t.Node({ "z", Kind::Affection, false }, 0));
    CHECK(!t.Node({ "a", Kind::Affection, false }, 100));
    const auto end = t.Stopped(kPair, 1000);
    CHECK(end && end->stoppedForMismatch);
}

TEST(Scene_OursIntimacyNeverMismatches)
{
    SceneTracker t;
    t.Adopt({ 7, Kind::Intimacy, "" });
    CHECK(!t.Node({ "z", Kind::Affection, false }, 0));
    CHECK(!t.Node({ "y", Kind::Intimacy, false }, 100));
    CHECK(!t.Node({ "x", Kind::Closeness, true }, 200));
    const auto end = t.Stopped(kPair, 1000);
    CHECK(end && end->ours);
    CHECK(end && end->oursKind == Kind::Intimacy);
    CHECK(end && !end->stoppedForMismatch);
}

TEST(Scene_AdoptBeforeAnyEvent)
{
    SceneTracker t;
    t.Adopt({ 7, Kind::Affection, "a" });
    CHECK(!t.Active());
    CHECK(!t.Node({ "a", Kind::Affection, false }, 0));
    t.Started(kPair, 10);
    const auto end = t.Stopped(kPair, 1000);
    CHECK(end && end->ours);
    CHECK(end && end->oursKind == Kind::Affection);

    // Adopt while a record exists marks that record.
    t.Started(kPair, 0);
    t.Adopt({ 7, Kind::Closeness, "b" });
    const auto second = t.Stopped(kPair, 1000);
    CHECK(second && second->ours);
    CHECK(second && second->oursKind == Kind::Closeness);

    // A remembered Adopt is forgotten by Clear.
    t.Adopt({ 7, Kind::Affection, "a" });
    t.Clear();
    t.Started(kPair, 0);
    const auto third = t.Stopped(kPair, 1000);
    CHECK(third && !third->ours);
}

TEST(Scene_FallbackTimer)
{
    SceneTracker t;
    t.Adopt({ 7, Kind::Affection, "a" });
    t.Started(kPair, 0);
    CHECK(!t.Node({ "a", Kind::Affection, false }, 0));
    CHECK(!t.OursGentleFor(39000, 40000));
    CHECK(t.OursGentleFor(40000, 40000));
    t.StopAsked();
    CHECK(!t.OursGentleFor(41000, 40000));
    (void)t.Stopped(kPair, 42000);

    t.Started(kPair, 0);
    CHECK(!t.OursGentleFor(50000, 40000));
    (void)t.Stopped(kPair, 51000);

    t.Adopt({ 7, Kind::Intimacy, "" });
    t.Started(kPair, 0);
    CHECK(!t.OursGentleFor(50000, 40000));
    (void)t.Stopped(kPair, 51000);

    CHECK(!t.OursGentleFor(50000, 40000));
}

TEST(Scene_NotOursAfterStop)
{
    SceneTracker t;
    t.Adopt({ 7, Kind::Affection, "a" });
    t.Started(kPair, 0);
    const auto first = t.Stopped(kPair, 1000);
    CHECK(first && first->ours);
    t.Started(kPair, 2000);
    const auto second = t.Stopped(kPair, 3000);
    CHECK(second && !second->ours);
    CHECK(second && !second->stoppedForMismatch);
}

TEST(Scene_SecondStartKeepsEarlierTime)
{
    SceneTracker t;
    t.Started({ 0x14 }, 0);
    t.Started(kPair, 5000);
    const auto end = t.Stopped(kPair, 30000);
    CHECK(end && end->seconds == 30.0f);
    CHECK(end && end->participants == kPair);
}

TEST(Scene_AdoptExistingRecordNextNodeIsFirst)
{
    SceneTracker t;
    t.Started(kPair, 0);
    CHECK(!t.Node({ "x", Kind::Affection, false }, 100));
    t.Adopt({ 7, Kind::Affection, "a" });
    CHECK(t.Node({ "z", Kind::Affection, false }, 200));
    const auto end = t.Stopped(kPair, 1000);
    CHECK(end && end->ours && end->stoppedForMismatch);
}

TEST(Scene_MissingNeedsTwoTicksInARow)
{
    SceneTracker t;
    CHECK(!t.Missing(false));
    CHECK(t.Missing(false));

    t.Clear();
    CHECK(!t.Missing(false));
    CHECK(!t.Missing(true));
    CHECK(!t.Missing(false));
    CHECK(t.Missing(false));

    t.Clear();
    CHECK(!t.Missing(false));
    t.Clear();
    CHECK(!t.Missing(false));

    // A stop starts the count again.
    t.Clear();
    t.Started(kPair, 0);
    CHECK(!t.Missing(false));
    (void)t.Stopped(kPair, 1000);
    CHECK(!t.Missing(false));
}

TEST(Scene_StaleEventsAfterStop)
{
    SceneTracker t;
    t.Started(kPair, 0, 1u);
    (void)t.Stopped(kPair, 1000, 1u);
    CHECK(!t.Node({ "a", Kind::Intimacy, false }, 1100, 1u));
    t.Started(kPair, 1100, 1u);
    CHECK(!t.Active());
    CHECK(!t.Stopped(kPair, 1200, 1u).has_value());

    CHECK(!t.Node({ "a", Kind::Affection, false }, 2000, 2u));
    CHECK(t.Active());
    const auto end = t.Stopped(kPair, 3000, 2u);
    CHECK(end && end->kind == Kind::Affection);
    CHECK(!t.Node({ "b", Kind::Affection, false }, 3100, 2u));
    CHECK(!t.Active());
}

TEST(Scene_InterfaceText)
{
    CHECK(InterfaceText(1) == "1");
    CHECK(InterfaceText(3) == "3");
    CHECK(InterfaceText(0x07050020) == "7.5.2");
    CHECK(InterfaceText(0x07050013) == "7.5.1.3");
    CHECK(InterfaceText(0x07050003) == "7.5.0.3");
}
