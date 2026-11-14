#include "check.h"

#include "ui/Whispers.h"

#include <limits>

using namespace romantasy::ui::whispers;

namespace
{
    constexpr std::uint64_t t0 = 100000;

    Whisper Deed(std::uint32_t follower, const char* label, std::int32_t points, bool strong = false)
    {
        Whisper w;
        w.kind = Kind::Deed;
        w.follower = follower;
        w.name = "Aelira";
        w.label = label;
        w.points = points;
        w.strong = strong;
        return w;
    }

    Whisper Plain(Kind kind, const char* label = "")
    {
        Whisper w;
        w.kind = kind;
        w.follower = 1;
        w.name = "Aelira";
        w.label = label;
        return w;
    }

    void CheckLine(const Whisper& w, const char* line, Tone tone, const char* icon)
    {
        const Shown s = Describe(w);
        CHECK(s.name == "Aelira");
        CHECK(s.line == line);
        CHECK(s.tone == tone);
        CHECK(s.icon == icon);
    }
}

TEST(Whispers_DescribeTable)
{
    CheckLine(Deed(1, "Locations Discovered", 4), "Locations Discovered \xC2\xB7 +4", Tone::Gain, "heart");
    CheckLine(Deed(1, "Lockpicking", 6, true), "Lockpicking \xC2\xB7 +6", Tone::Gain, "star");
    CheckLine(Deed(1, "Murders", -5), "Murders \xC2\xB7 \xE2\x88\x92" "5", Tone::Loss, "dislikes");
    CheckLine(Deed(1, "Murders", -9, true), "Murders \xC2\xB7 \xE2\x88\x92" "9", Tone::Loss, "heartbroken");

    Whisper merged = Deed(1, "x", 7);
    merged.deeds = 3;
    CheckLine(merged, "3 deeds \xC2\xB7 +7", Tone::Gain, "heart");
    merged.points = -7;
    CheckLine(merged, "3 deeds \xC2\xB7 \xE2\x88\x92" "7", Tone::Loss, "dislikes");

    CheckLine(Plain(Kind::MoodWarm), "Feels warm toward you", Tone::Gain, "heart");
    CheckLine(Plain(Kind::MoodStrained), "Things are strained", Tone::Loss, "dislikes");
    CheckLine(Plain(Kind::Reunion), "Glad to have you back", Tone::Gain, "heart");
    CheckLine(Plain(Kind::TimeApart), "Has been away from you for days", Tone::Plain, "neutral");
    CheckLine(Plain(Kind::TierGained, "Friend"), "Reached Friend", Tone::Gain, "heart");
    CheckLine(Plain(Kind::TierLost, "Acquaintance"), "Fell to Acquaintance", Tone::Loss, "dislikes");
}

TEST(Whispers_DescribeSurvivesBadText)
{
    constexpr auto lo = std::numeric_limits<std::int32_t>::min();
    Whisper w = Deed(1, "", lo);
    CHECK(Describe(w).line == "A deed \xC2\xB7 \xE2\x88\x92" "2147483648");

    // Long, with a two-byte letter that would straddle the cut.
    w.name = std::string(63, 'a') + "\xC3\xA9" + "tail";
    w.label = std::string(79, 'b') + "\xC3\xA9";
    w.points = 1;
    const Shown s = Describe(w);
    CHECK(s.name == std::string(63, 'a'));
    CHECK(s.line == std::string(79, 'b') + " \xC2\xB7 +1");

    // Invalid UTF-8 becomes U+FFFD; truncated tail of a sequence too.
    w.name = "A\xFF" "B\xC3";
    w.label = "\xE2\x88";
    const Shown bad = Describe(w);
    CHECK(bad.name == "A\xEF\xBF\xBD" "B\xEF\xBF\xBD");
    CHECK(bad.line == "\xEF\xBF\xBD\xEF\xBF\xBD \xC2\xB7 +1");

    Whisper empty;
    empty.kind = Kind::TierGained;
    CHECK(Describe(empty).line == "Reached a new tier");
    CHECK(NotificationText(empty) == "Reached a new tier");
}

TEST(Whispers_NotificationText)
{
    CHECK(NotificationText(Deed(1, "Locations Discovered", 4)) == "Aelira: Locations Discovered \xC2\xB7 +4");
    CHECK(NotificationText(Plain(Kind::Reunion)) == "Aelira: Glad to have you back");
    CHECK(NotificationText(Plain(Kind::TierLost, "Friend")) == "Aelira: Fell to Friend");
}

TEST(Whispers_MergesDeeds)
{
    // Five deeds for one follower inside two seconds: one whisper with the net.
    Queue q;
    q.Push(Deed(1, "A", 3), t0);
    q.Push(Deed(1, "B", 2, true), t0 + 500);
    q.Push(Deed(1, "C", -1), t0 + 1000);
    q.Push(Deed(1, "D", 4), t0 + 2500);  // 1500 after the last push: still merges
    q.Push(Deed(1, "E", 1), t0 + 3000);
    CHECK(q.Waiting() == 1);
    const auto w = q.Next(t0 + 3000, true);
    CHECK(w.has_value());
    CHECK(w->points == 9);
    CHECK(w->deeds == 5);
    CHECK(w->strong);
    CHECK(w->label == "E");

    // A net of 0 gives none.
    Queue z;
    z.Push(Deed(1, "A", 5), t0);
    z.Push(Deed(1, "B", -5), t0 + 100);
    CHECK(z.Waiting() == 0);
    CHECK(!z.Next(t0 + 100, true).has_value());
    // ... and the next deed starts fresh.
    z.Push(Deed(1, "C", 2), t0 + 200);
    CHECK(z.Waiting() == 1);
    CHECK(z.Next(t0 + 200, true)->deeds == 1);

    // Two followers stay separate.
    Queue two;
    two.Push(Deed(1, "A", 3), t0);
    two.Push(Deed(2, "A", 3), t0 + 10);
    two.Push(Deed(1, "B", 3), t0 + 20);
    CHECK(two.Waiting() == 2);

    // A lone deed with no points says nothing.
    Queue none;
    none.Push(Deed(1, "A", 0), t0);
    CHECK(none.Waiting() == 0);
}

TEST(Whispers_MergeSumsClamp)
{
    constexpr auto hi = std::numeric_limits<std::int32_t>::max();
    constexpr auto lo = std::numeric_limits<std::int32_t>::min();
    Queue q;
    q.Push(Deed(1, "A", hi), t0);
    Whisper b = Deed(1, "B", hi);
    b.deeds = hi;
    q.Push(b, t0 + 1);
    auto w = q.Next(t0 + 1, true);
    CHECK(w->points == hi);
    CHECK(w->deeds == hi);

    Queue n;
    n.Push(Deed(1, "A", lo), t0);
    n.Push(Deed(1, "B", lo), t0 + 1);
    CHECK(n.Next(t0 + 1, true)->points == lo);
}

TEST(Whispers_MergeWindowCloses)
{
    Queue q;
    q.Push(Deed(1, "A", 3), t0);
    q.Push(Deed(1, "B", 3), t0 + 2000);  // exactly the window: merges
    CHECK(q.Waiting() == 1);
    q.Push(Deed(1, "C", 3), t0 + 4001);  // 2001 after the last push
    CHECK(q.Waiting() == 2);
}

TEST(Whispers_OnlyDeedsMerge)
{
    Queue q;
    q.Push(Plain(Kind::MoodWarm), t0);
    q.Push(Plain(Kind::MoodWarm), t0 + 1);
    q.Push(Deed(1, "A", 3), t0 + 2);
    q.Push(Plain(Kind::Reunion), t0 + 3);
    q.Push(Deed(1, "A", 3), t0 + 4);  // merges into the waiting deed of the same follower, with the reunion between them
    CHECK(q.Waiting() == 4);
    CHECK(q.Next(t0 + 4, true)->kind == Kind::MoodWarm);
}

TEST(Whispers_CapDropsOldest)
{
    Queue q;
    for (std::uint32_t i = 1; i <= 5; ++i) q.Push(Deed(i, "A", static_cast<std::int32_t>(i)), t0 + i);
    CHECK(q.Waiting() == kMaxWaiting);
    CHECK(q.Next(t0 + 10, true)->follower == 2);
}

TEST(Whispers_AgeDropsWaiting)
{
    Queue q;
    q.Push(Deed(1, "A", 3), t0);
    q.Push(Deed(2, "A", 3), t0 + 20000);
    CHECK(!q.Next(t0 + 29999, false).has_value());
    CHECK(q.Waiting() == 2);
    CHECK(!q.Next(t0 + 30000, false).has_value());  // the first is dropped, not shown late
    CHECK(q.Waiting() == 1);
    CHECK(q.Next(t0 + 30001, true)->follower == 2);

    // A merged entry ages from when it began to wait.
    Queue m;
    m.Push(Deed(1, "A", 3), t0);
    for (int i = 1; i <= 20; ++i) m.Push(Deed(1, "A", 1), t0 + i * 1500);
    CHECK(m.Waiting() == 1);
    CHECK(!m.Next(t0 + 30000, true).has_value());
    CHECK(m.Waiting() == 0);
}

TEST(Whispers_OneAtATime)
{
    Queue q;
    q.Push(Deed(1, "A", 3), t0);
    q.Push(Deed(2, "A", 3), t0 + 1);
    CHECK(!q.Showing(t0));
    CHECK(q.Next(t0 + 10, true)->follower == 1);
    CHECK(q.Showing(t0 + 10));
    // However often it is asked, nothing comes while one is showing.
    for (std::uint64_t t = t0 + 11; t < t0 + 5000; t += 7) CHECK(!q.Next(t, true).has_value());
    CHECK(q.Waiting() == 1);
    q.Finished();
    CHECK(!q.Showing(t0 + 5000));
    CHECK(q.Next(t0 + 5000, true)->follower == 2);
    CHECK(!q.Next(t0 + 5001, true).has_value());
}

TEST(Whispers_AnswerTimeout)
{
    Queue q;
    q.Push(Deed(1, "A", 3), t0);
    q.Push(Deed(2, "A", 3), t0);
    CHECK(q.Next(t0, true).has_value());
    CHECK(!q.Next(t0 + kAnswerMs - 1, true).has_value());
    CHECK(q.Showing(t0 + kAnswerMs - 1));
    CHECK(!q.Showing(t0 + kAnswerMs));  // const check agrees before Next runs
    CHECK(q.Next(t0 + kAnswerMs, true)->follower == 2);

    // The clock jumped back: the shown whisper is not immortal and not instantly gone.
    Queue b;
    b.Push(Deed(1, "A", 3), t0);
    b.Push(Deed(2, "A", 3), t0);
    CHECK(b.Next(t0, true).has_value());
    CHECK(!b.Next(t0 - 50000, true).has_value());
    CHECK(b.Showing(t0 - 50000));
    CHECK(!b.Next(t0 - 50000 + kAnswerMs - 1, true).has_value());
    CHECK(b.Next(t0 - 50000 + kAnswerMs, true)->follower == 2);

    // A jump forward (a long pause) frees the queue at once.
    Queue f;
    f.Push(Deed(1, "A", 3), t0);
    CHECK(f.Next(t0, true).has_value());
    f.Push(Deed(2, "A", 3), t0 + 3600000);
    CHECK(f.Next(t0 + 3600000, true)->follower == 2);
}

TEST(Whispers_ClockBackwardsWaiting)
{
    Queue q;
    q.Push(Deed(1, "A", 3), t0);
    CHECK(!q.Next(t0 - 10000, false).has_value());  // no underflow: age 0, kept
    CHECK(q.Waiting() == 1);
    CHECK(!q.Next(t0 - 10000 + 29999, false).has_value());
    CHECK(q.Waiting() == 1);
    CHECK(!q.Next(t0 - 10000 + 30000, false).has_value());  // not immortal
    CHECK(q.Waiting() == 0);

    // A push after the clock went back still merges into the waiting deed.
    Queue m;
    m.Push(Deed(1, "A", 3), t0);
    m.Push(Deed(1, "B", 3), t0 - 100000);
    CHECK(m.Waiting() == 1);
    CHECK(m.Next(t0 - 100000, true)->deeds == 2);
}

TEST(Whispers_HeldWhileCannotShow)
{
    Queue q;
    q.Push(Deed(1, "A", 3), t0);
    for (std::uint64_t t = t0; t < t0 + 20000; t += 1000) CHECK(!q.Next(t, false).has_value());
    CHECK(q.Waiting() == 1);
    CHECK(q.Next(t0 + 20000, true).has_value());  // the menu closed

    // Held past thirty seconds: dropped, not shown late.
    Queue late;
    late.Push(Deed(1, "A", 3), t0);
    CHECK(!late.Next(t0 + 29000, false).has_value());
    CHECK(!late.Next(t0 + 31000, true).has_value());
    CHECK(late.Waiting() == 0);
}

TEST(Whispers_DisabledIgnoresPush)
{
    Queue q;
    q.Push(Deed(1, "A", 3), t0);
    CHECK(q.Next(t0, true).has_value());
    q.Push(Deed(2, "A", 3), t0);
    q.SetEnabled(false);
    CHECK(q.Waiting() == 0);
    CHECK(!q.Showing(t0));
    q.Push(Deed(1, "A", 3), t0 + 1);
    q.Push(Plain(Kind::Reunion), t0 + 1);
    CHECK(q.Waiting() == 0);
    CHECK(!q.Next(t0 + 1, true).has_value());

    // Off then on starts clean.
    q.SetEnabled(true);
    CHECK(q.Waiting() == 0);
    CHECK(!q.Showing(t0 + 2));
    q.Push(Deed(1, "A", 3), t0 + 2);
    CHECK(q.Next(t0 + 2, true).has_value());
}

TEST(Whispers_ClearForgetsShown)
{
    Queue q;
    q.Push(Deed(1, "A", 3), t0);
    q.Push(Deed(2, "A", 3), t0);
    CHECK(q.Next(t0, true).has_value());
    q.Clear();
    CHECK(q.Waiting() == 0);
    CHECK(!q.Showing(t0));
    q.Push(Deed(3, "A", 3), t0 + 1);
    CHECK(q.Next(t0 + 1, true)->follower == 3);
}

TEST(Whispers_CornerKeys)
{
    const std::pair<const char*, WhisperCorner> corners[] = {
        { "topLeft", WhisperCorner::TopLeft },
        { "topRight", WhisperCorner::TopRight },
        { "bottomLeft", WhisperCorner::BottomLeft },
        { "bottomRight", WhisperCorner::BottomRight },
    };
    for (const auto& [key, corner] : corners) {
        CHECK(ParseWhisperCorner(key) == corner);
        CHECK(Key(corner) == key);
    }
    CHECK(!ParseWhisperCorner(""));
    CHECK(!ParseWhisperCorner("TopRight"));
    CHECK(!ParseWhisperCorner("topright"));
    CHECK(!ParseWhisperCorner("top right"));
    CHECK(!ParseWhisperCorner("topRight "));
    CHECK(Key(static_cast<WhisperCorner>(9)) == "topRight");
}

TEST(Whispers_PlaceCardAt1080)
{
    // 1920x1080: scale 1, the card 420x96, 32 from the side, 96 from the top, 32 from the bottom.
    const auto tr = PlaceCard(1920, 1080, WhisperCorner::TopRight);
    CHECK(tr.has_value());
    CHECK(tr && tr->x == 1920 - 32 - 420 && tr->y == 96 && tr->w == 420 && tr->h == 96 && tr->scale == 1.0f);
    const auto tl = PlaceCard(1920, 1080, WhisperCorner::TopLeft);
    CHECK(tl && tl->x == 32 && tl->y == 96);
    const auto bl = PlaceCard(1920, 1080, WhisperCorner::BottomLeft);
    CHECK(bl && bl->x == 32 && bl->y == 1080 - 32 - 96);
    const auto br = PlaceCard(1920, 1080, WhisperCorner::BottomRight);
    CHECK(br && br->x == 1920 - 32 - 420 && br->y == 1080 - 32 - 96);
}

TEST(Whispers_PlaceCardScales)
{
    // 4K: scale 2, everything doubled.
    const auto k4 = PlaceCard(3840, 2160, WhisperCorner::TopRight);
    CHECK(k4 && k4->scale == 2.0f && k4->w == 840 && k4->h == 192 && k4->x == 3840 - 64 - 840 && k4->y == 192);
    // Ultrawide 3440x1440: scale 4/3, measured from the right edge.
    const auto uw = PlaceCard(3440, 1440, WhisperCorner::BottomRight);
    CHECK(uw && std::fabs(uw->scale - 1440.0f / 1080.0f) < 1e-5f && uw->w >= 560 && uw->w <= 561 && uw->h >= 128 && uw->h <= 129);
    CHECK(uw && uw->x == 3440 - 43 - uw->w && uw->y == 1440 - 43 - uw->h);
    // Smaller than 1080 never shrinks; above 3240 the scale stops at 3.
    const auto small = PlaceCard(1280, 720, WhisperCorner::TopLeft);
    CHECK(small && small->scale == 1.0f && small->w == 420 && small->h == 96 && small->x == 32 && small->y == 96);
    const auto huge = PlaceCard(7680, 4320, WhisperCorner::TopLeft);
    CHECK(huge && huge->scale == 3.0f && huge->w == 1260 && huge->h == 288);
}

TEST(Whispers_PlaceCardNeedsADisplay)
{
    CHECK(!PlaceCard(0, 0, WhisperCorner::TopRight));
    CHECK(!PlaceCard(1920, 0, WhisperCorner::TopRight));
    CHECK(!PlaceCard(-1, 1080, WhisperCorner::TopRight));
    // A display narrower or shorter than the card keeps it on screen from the top-left.
    const auto tiny = PlaceCard(300, 100, WhisperCorner::BottomRight);
    CHECK(tiny && tiny->x == 0 && tiny->y == 0);
}

TEST(Whispers_CanShow)
{
    CHECK(CanShow({}));
    CHECK(!CanShow({ .paused = true }));
    CHECK(!CanShow({ .menuOpen = true }));
    CHECK(!CanShow({ .inDialogue = true }));
    CHECK(!CanShow({ .ownViewVisible = true }));
    CHECK(!CanShow({ .worldReady = false }));
}

TEST(Show_SceneBlocks)
{
    CHECK(!CanShow({ .inScene = true }));
    CHECK(CanShow({ .inScene = false }));
    CHECK(!CanShow({ .paused = true, .inScene = false }));
}

TEST(Whispers_MenuBlocks)
{
    // The HUD family never blocks, whatever its flags.
    CHECK(!MenuBlocks({ .name = "HUD Menu", .pausesGame = true, .usesCursor = true }));
    CHECK(!MenuBlocks({ .name = "Fader Menu", .modal = true }));
    CHECK(!MenuBlocks({ .name = "Cursor Menu", .usesCursor = true }));
    // Dialogue, the main menu and loading always block.
    CHECK(MenuBlocks({ .name = "Dialogue Menu" }));
    CHECK(MenuBlocks({ .name = "Main Menu" }));
    CHECK(MenuBlocks({ .name = "Loading Menu" }));
    // Any other menu blocks when it pauses or takes input.
    CHECK(MenuBlocks({ .name = "InventoryMenu", .pausesGame = true }));
    CHECK(MenuBlocks({ .name = "Favorites Menu", .usesMenuContext = true }));
    CHECK(MenuBlocks({ .name = "SomeModMenu", .usesCursor = true }));
    CHECK(MenuBlocks({ .name = "SomeModMenu", .modal = true }));
    // An always-open widget menu from another mod does not.
    CHECK(!MenuBlocks({ .name = "TrueHUD" }));
}

TEST(Whispers_AnswerSeq)
{
    CHECK(IsCurrentAnswer(5, 5));
    CHECK(!IsCurrentAnswer(4, 5));   // a late answer for a whisper already timed out
    CHECK(!IsCurrentAnswer(0, 0));   // nothing on screen: no answer counts
    CHECK(!IsCurrentAnswer(5, 0));
    CHECK(NextSeq(0) == 1);
    CHECK(NextSeq(1) == 2);
    CHECK(NextSeq(2147483646u) == 2147483647u);
    CHECK(NextSeq(2147483647u) == 1);  // the page accepts 1..2147483647
    CHECK(NextSeq(0xFFFFFFFFu) == 1);
}

TEST(Whispers_MoodCrossing)
{
    using romantasy::mood::State;
    CHECK(MoodCrossing(State::Settled, State::Warm) == Kind::MoodWarm);
    CHECK(MoodCrossing(State::Strained, State::Warm) == Kind::MoodWarm);
    CHECK(MoodCrossing(State::Settled, State::Strained) == Kind::MoodStrained);
    CHECK(MoodCrossing(State::Warm, State::Strained) == Kind::MoodStrained);
    CHECK(!MoodCrossing(State::Warm, State::Settled));      // back to Settled says nothing
    CHECK(!MoodCrossing(State::Strained, State::Settled));
    CHECK(!MoodCrossing(State::Warm, State::Warm));         // no crossing
    CHECK(!MoodCrossing(State::Strained, State::Strained));
}

namespace
{
    Whisper Note(std::uint32_t follower, Kind kind = Kind::Reunion)
    {
        Whisper w;
        w.kind = kind;
        w.follower = follower;
        w.name = "Aelira";
        return w;
    }
    using What = Pump::Decision::What;
}

TEST(Whispers_NotificationMarkup)
{
    Whisper w = Deed(1, "Bread & Salt", -5);
    w.name = "<b>Aelira</b>";
    CHECK(NotificationMarkup(w) == "&lt;b>Aelira&lt;/b>: Bread &amp; Salt \xC2\xB7 -5");
    CHECK(NotificationMarkup(Deed(1, "Murders", -9, true)) == "Aelira: Murders \xC2\xB7 -9");
    CHECK(NotificationMarkup(Deed(1, "Lockpicking", 6)) == "Aelira: Lockpicking \xC2\xB7 +6");
    CHECK(NotificationMarkup(Plain(Kind::TierLost, "Friend")) == "Aelira: Fell to Friend");
}

TEST(Whispers_PumpLateAnswerDoesNotEndTheNext)
{
    Pump pump;
    pump.Push(Note(1), t0);
    const auto a = pump.Next(t0, true, true);
    CHECK(a.what == What::Show && a.seq == 1);
    pump.Push(Note(2), t0 + 100);
    CHECK(pump.Next(t0 + 7999, true, true).what == What::None);  // A still up, B waits
    const auto b = pump.Next(t0 + kAnswerMs, true, true);         // A unanswered for 8 s: B goes up
    CHECK(b.what == What::Show && b.seq == 2 && b.whisper.follower == 2);
    CHECK(!pump.Answer(1));                                       // A's late answer
    CHECK(pump.Current() == 2);
    CHECK(pump.Next(t0 + kAnswerMs + 1, true, true).what == What::None);
}

TEST(Whispers_PumpDoubleAnswerAdvancesOnce)
{
    Pump pump;
    pump.Push(Note(1), t0);
    pump.Push(Note(2), t0);
    pump.Push(Note(3), t0);
    CHECK(pump.Next(t0, true, true).seq == 1);
    CHECK(pump.Answer(1));
    const auto b = pump.Next(t0 + 10, true, true);
    CHECK(b.what == What::Show && b.seq == 2 && b.whisper.follower == 2);
    CHECK(!pump.Answer(1));                                       // the same answer again
    CHECK(pump.Next(t0 + 20, true, true).what == What::None);     // C still waits behind B
    CHECK(pump.Current() == 2);
    CHECK(pump.Answer(2));
    const auto c = pump.Next(t0 + 30, true, true);
    CHECK(c.what == What::Show && c.whisper.follower == 3);
    CHECK(pump.Answer(3));
    const auto none = pump.Next(t0 + 40, true, true);
    CHECK(none.what == What::None && none.hide && !pump.Pending());
}

TEST(Whispers_PumpIgnoresZeroAndWrongSeq)
{
    Pump pump;
    CHECK(!pump.Answer(0));                                       // nothing on the card
    pump.Push(Note(1), t0);
    pump.Push(Note(2), t0);
    CHECK(pump.Next(t0, true, true).seq == 1);
    CHECK(!pump.Answer(0));
    CHECK(!pump.Answer(7));
    CHECK(pump.Current() == 1);
    const auto still = pump.Next(t0 + 10, true, true);
    CHECK(still.what == What::None && !still.hide);
}

TEST(Whispers_PumpWithoutViewNotifiesInOrder)
{
    Pump pump;
    pump.Push(Note(1), t0);
    pump.Push(Note(2, Kind::TimeApart), t0);
    pump.Push(Note(3), t0);
    for (std::uint32_t follower = 1; follower <= 3; ++follower) {
        const auto d = pump.Next(t0, true, false);
        CHECK(d.what == What::Notify && d.whisper.follower == follower);
    }
    const auto none = pump.Next(t0, true, false);
    CHECK(none.what == What::None && !pump.Pending());
    // A Show the card cannot take is finished at once and the next comes after it.
    pump.Push(Note(4), t0);
    pump.Push(Note(5), t0);
    CHECK(pump.Next(t0, true, true).what == What::Show);
    pump.ShowFailed();
    CHECK(pump.Current() == 0);
    CHECK(pump.Next(t0, true, true).whisper.follower == 5);
}

TEST(Whispers_PumpAgesOutWhileBlocked)
{
    Pump pump;
    pump.Push(Note(1), t0);
    pump.Push(Note(2), t0 + 10000);
    const auto blocked = pump.Next(t0 + kMaxAgeMs - 1, false, true);
    CHECK(blocked.what == What::None && blocked.hide && pump.Pending());
    CHECK(pump.Next(t0 + kMaxAgeMs, false, true).what == What::None);  // the first is 30 s old: dropped
    const auto young = pump.Next(t0 + kMaxAgeMs, true, true);
    CHECK(young.what == What::Show && young.whisper.follower == 2);
}

TEST(Whispers_PumpOffDropsEverything)
{
    Pump pump;
    pump.Push(Note(1), t0);
    pump.Push(Note(2), t0);
    CHECK(pump.Next(t0, true, true).seq == 1);
    pump.SetEnabled(false);
    CHECK(pump.Current() == 0 && !pump.Pending());
    CHECK(!pump.Answer(1));
    pump.Push(Note(3), t0 + 10);
    const auto off = pump.Next(t0 + 10, true, true);
    CHECK(off.what == What::None && off.hide);
    pump.SetEnabled(true);
    pump.Push(Note(4), t0 + 20);
    const auto on = pump.Next(t0 + 20, true, true);
    CHECK(on.what == What::Show && on.whisper.follower == 4 && on.seq == 2);
}

TEST(Whispers_PlaceCardCoversTheCard)
{
    for (const std::int32_t height : { 1080, 1200, 1440, 1600, 2160 }) {
        for (const std::int32_t width : { height * 16 / 9, height * 16 / 10 }) {
            for (const auto corner : { WhisperCorner::TopLeft, WhisperCorner::TopRight, WhisperCorner::BottomLeft, WhisperCorner::BottomRight }) {
                const auto p = PlaceCard(width, height, corner);
                CHECK(p.has_value());
                if (!p) continue;
                CHECK(static_cast<float>(p->w) / p->scale >= static_cast<float>(kCardWidth));
                CHECK(static_cast<float>(p->h) / p->scale >= static_cast<float>(kCardHeight));
                CHECK(p->x >= 0 && p->y >= 0 && p->x + p->w <= width && p->y + p->h <= height);
            }
        }
    }
}

TEST(Whispers_PumpGivesUpAfterTwoSilentCards)
{
    Pump pump;
    pump.Push(Note(1), t0);
    CHECK(pump.Next(t0, true, true).seq == 1);
    pump.Push(Note(2), t0 + 10);
    pump.Push(Note(3), t0 + 20);
    const auto first = pump.Next(t0 + kAnswerMs, true, true);  // 1 unanswered: 2 goes up
    CHECK(first.timedOut == 1 && !first.gaveUp && first.what == What::Show && first.whisper.follower == 2);
    pump.Push(Note(4), t0 + kAnswerMs + 10);
    const auto second = pump.Next(t0 + 2 * kAnswerMs, true, true);  // 2 unanswered too: the card is given up
    CHECK(second.timedOut == first.seq && second.gaveUp && pump.GaveUp());
    CHECK(second.what == What::Notify && second.whisper.follower == 3);
    const auto rest = pump.Next(t0 + 2 * kAnswerMs, true, true);
    CHECK(rest.what == What::Notify && rest.whisper.follower == 4 && !rest.gaveUp);
    CHECK(pump.Next(t0 + 2 * kAnswerMs, true, true).what == What::None);
    pump.Push(Note(5), t0 + 2 * kAnswerMs + 10);
    CHECK(pump.Next(t0 + 2 * kAnswerMs + 10, true, true).what == What::Notify);  // for the session
}

TEST(Whispers_PumpAnswerResetsTheSilentCount)
{
    Pump pump;
    pump.Push(Note(1), t0);
    CHECK(pump.Next(t0, true, true).what == What::Show);
    pump.Push(Note(2), t0 + 10);
    const auto b = pump.Next(t0 + kAnswerMs, true, true);   // 1 timed out
    CHECK(b.timedOut == 1 && b.what == What::Show);
    CHECK(pump.Answer(b.seq));                               // 2 answered
    pump.Push(Note(3), t0 + kAnswerMs + 10);
    const auto c = pump.Next(t0 + kAnswerMs + 10, true, true);
    CHECK(c.what == What::Show);
    const auto after = pump.Next(t0 + 2 * kAnswerMs + 10, true, true);  // 3 timed out: one in a row
    CHECK(after.timedOut == c.seq && !after.gaveUp && !pump.GaveUp());
    pump.Push(Note(4), t0 + 2 * kAnswerMs + 20);
    CHECK(pump.Next(t0 + 2 * kAnswerMs + 20, true, true).what == What::Show);
}

TEST(Whispers_PumpLostCardIsGivenBack)
{
    Pump pump;
    CHECK(!pump.Lost());
    pump.Push(Note(1), t0);
    pump.Push(Note(2), t0);
    CHECK(pump.Next(t0, true, true).what == What::Show);
    const auto lost = pump.Lost();
    CHECK(lost && lost->follower == 1 && pump.Current() == 0);
    CHECK(!pump.Answer(1));
    // The view is gone: the waiting ones follow through the fallback.
    const auto next = pump.Next(t0 + 10, true, false);
    CHECK(next.what == What::Notify && next.whisper.follower == 2 && next.timedOut == 0);
}

TEST(Whispers_PumpMenuMidCard)
{
    Pump pump;
    pump.Push(Note(1), t0);
    pump.Push(Note(2), t0);
    CHECK(pump.Next(t0, true, true).seq == 1);
    const auto blocked = pump.Next(t0 + 100, false, true);   // a menu opens: the card is hidden
    CHECK(blocked.what == What::None && blocked.hide && pump.Current() == 1);
    const auto back = pump.Next(t0 + 200, true, true);       // it closes before the answer
    CHECK(back.what == What::None && !back.hide && pump.Current() == 1);  // no second Show of the same whisper
    // The next one waits for the answer ...
    CHECK(pump.Answer(1));
    CHECK(pump.Next(t0 + 300, true, true).whisper.follower == 2);
    // ... or for the 8 second rule.
    Pump silent;
    silent.Push(Note(1), t0);
    silent.Push(Note(2), t0);
    CHECK(silent.Next(t0, true, true).seq == 1);
    CHECK(silent.Next(t0 + 100, false, true).hide);
    CHECK(silent.Next(t0 + kAnswerMs - 1, true, true).what == What::None);
    const auto late = silent.Next(t0 + kAnswerMs, true, true);
    CHECK(late.timedOut == 1 && late.what == What::Show && late.whisper.follower == 2);
}

TEST(Whispers_PumpHiddenCardIsNotSilent)
{
    // Two cards in a row time out while a menu hides them: not the page's fault, no give-up.
    Pump pump;
    pump.Push(Note(1), t0);
    CHECK(pump.Next(t0, true, true).seq == 1);
    CHECK(pump.Next(t0 + 100, false, true).hide);                 // a menu opens
    pump.Push(Note(2), t0 + 200);
    const auto first = pump.Next(t0 + kAnswerMs, true, true);    // it closes after 8 s
    CHECK(first.timedOut == 1 && first.timedOutBlocked && !first.gaveUp && first.what == What::Show);
    CHECK(pump.Next(t0 + kAnswerMs + 100, false, true).hide);     // another menu over the next card
    pump.Push(Note(3), t0 + kAnswerMs + 200);
    const auto second = pump.Next(t0 + 2 * kAnswerMs, true, true);
    CHECK(second.timedOut == first.seq && second.timedOutBlocked && !second.gaveUp && !pump.GaveUp());
    CHECK(second.what == What::Show);
}

TEST(Whispers_PumpHiddenCardDoesNotResetTheCount)
{
    Pump pump;
    pump.Push(Note(1), t0);
    CHECK(pump.Next(t0, true, true).seq == 1);
    pump.Push(Note(2), t0 + 10);
    const auto a = pump.Next(t0 + kAnswerMs, true, true);        // 1 silent, never hidden: count 1
    CHECK(a.timedOut == 1 && !a.timedOutBlocked && a.what == What::Show);
    CHECK(pump.Next(t0 + kAnswerMs + 100, false, true).hide);     // 2 hidden by a menu
    pump.Push(Note(3), t0 + kAnswerMs + 200);
    const auto b = pump.Next(t0 + 2 * kAnswerMs, true, true);    // 2 times out hidden: count stays 1
    CHECK(b.timedOutBlocked && !b.gaveUp && b.what == What::Show);
    pump.Push(Note(4), t0 + 2 * kAnswerMs + 10);
    const auto c = pump.Next(t0 + 3 * kAnswerMs, true, true);    // 3 silent, never hidden: count 2
    CHECK(c.timedOut == b.seq && !c.timedOutBlocked && c.gaveUp && pump.GaveUp());
    CHECK(c.what == What::Notify && c.whisper.follower == 4);
}

TEST(Whispers_ComfortFields)
{
    CHECK(ComfortFields(false, false).empty());   // off: the fields are left out
    CHECK((ComfortFields(true, false) == std::vector<ComfortField>{ { "contrast", "high" } }));
    CHECK((ComfortFields(false, true) == std::vector<ComfortField>{ { "motion", "reduced" } }));
    CHECK((ComfortFields(true, true) == std::vector<ComfortField>{ { "contrast", "high" }, { "motion", "reduced" } }));
}

TEST(Whispers_MomentRefusals)
{
    // A start the bond is not ready for, and one refused for a strained mood: the follower's name above a short line.
    CheckLine(Plain(Kind::MomentNotYet), "Not yet", Tone::Plain, "neutral");
    CheckLine(Plain(Kind::MomentNotInMood), "Not in the mood", Tone::Loss, "dislikes");
    CHECK(NotificationText(Plain(Kind::MomentNotYet)) == "Aelira: Not yet");
    CHECK(NotificationText(Plain(Kind::MomentNotInMood)) == "Aelira: Not in the mood");
    CHECK(KindName(Kind::MomentNotYet) == "moment not yet");
    CHECK(KindName(Kind::MomentNotInMood) == "moment not in the mood");
}

TEST(Whispers_MomentRefusalsQueueLikeAReunion)
{
    // Never merged, never dropped for having no points, dropped only by age or the cap, as a reunion is.
    Queue q;
    q.Push(Plain(Kind::MomentNotYet), t0);
    q.Push(Plain(Kind::MomentNotYet), t0 + 1);
    q.Push(Plain(Kind::MomentNotInMood), t0 + 2);
    q.Push(Plain(Kind::Reunion), t0 + 3);
    CHECK(q.Waiting() == 4);
    const auto first = q.Next(t0 + 3, true);
    CHECK(first.has_value() && first->kind == Kind::MomentNotYet);

    Queue aged;
    aged.Push(Plain(Kind::MomentNotInMood), t0);
    CHECK(!aged.Next(t0 + kMaxAgeMs, false).has_value());
    CHECK(aged.Waiting() == 0);

    Queue held;
    held.Push(Plain(Kind::MomentNotInMood), t0);
    CHECK(!held.Next(t0 + kMaxAgeMs - 1, false).has_value());
    const auto late = held.Next(t0 + kMaxAgeMs - 1, true);
    CHECK(late.has_value() && late->kind == Kind::MomentNotInMood);
}
