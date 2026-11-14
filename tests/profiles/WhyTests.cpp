#include "check.h"
#include "romance/Why.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

using namespace romantasy::why;

namespace
{
    constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
    constexpr std::int32_t kMin = std::numeric_limits<std::int32_t>::min();

    bool OneLine(const std::string& s)
    {
        for (const unsigned char c : s) {
            if (c < 0x20 || c == 0x7F) return false;
        }
        return s.find("\xE2\x80\xA8") == std::string::npos && s.find("\xE2\x80\xA9") == std::string::npos;
    }

    Deed Base()
    {
        Deed d;
        d.name = "Ambellina";
        d.deed = "Dungeons Cleared";
        d.count = 2;
        d.rule = 3;
        d.weight = 1;
        d.pace = 0;
        d.applied = 6;
        d.oldPoints = 490;
        d.newPoints = 496;
        d.tier = "Stranger";
        return d;
    }
}

TEST(Why_FavoriteOnFast)
{
    Deed d = Base();
    d.weight = 2;
    d.pace = 1;
    d.applied = 24;
    d.oldPoints = 490;
    d.newPoints = 514;
    d.tier = "Acquaintance";
    CHECK(Changed(d) == "why: Ambellina | Dungeons Cleared x2 | rule 3 | favorite x2 | fast x2 | +24 | 490 -> 514 Acquaintance");
}

TEST(Why_DislikeOnSlow)
{
    Deed d = Base();
    d.rule = 4;
    d.weight = -1;
    // A loss is never paced, so the line names no pace even on Slow burn.
    d.pace = PaceShown(-1, -8);
    d.applied = -8;
    d.oldPoints = 100;
    d.newPoints = 92;
    CHECK(Changed(d) == "why: Ambellina | Dungeons Cleared x2 | rule 4 | dislike | -8 | 100 -> 92 Stranger");
}

TEST(Why_LikeOnStandardOmitsPace)
{
    CHECK(Changed(Base()) == "why: Ambellina | Dungeons Cleared x2 | rule 3 | like | +6 | 490 -> 496 Stranger");
}

TEST(Why_PetPeeve)
{
    Deed d = Base();
    d.weight = -2;
    d.applied = -12;
    d.oldPoints = 20;
    d.newPoints = 8;
    CHECK(Changed(d) == "why: Ambellina | Dungeons Cleared x2 | rule 3 | pet peeve x2 | -12 | 20 -> 8 Stranger");
}

TEST(Why_NothingReasons)
{
    CHECK(Unchanged("Ina", "Locks Picked", 1, Nothing::NotFollowing) == "why: Ina | Locks Picked x1 | nothing: not following");
    CHECK(Unchanged("Ina", "Locks Picked", 4, Nothing::NoFeeling) == "why: Ina | Locks Picked x4 | nothing: no feeling about this deed");
    CHECK(Unchanged("Ina", "Locks Picked", 1, Nothing::NoRule) == "why: Ina | Locks Picked x1 | nothing: no rule for this deed");
    CHECK(Unchanged("Ina", "Locks Picked", 1, Nothing::RoundedToZero) == "why: Ina | Locks Picked x1 | nothing: rounded to 0 by slow burn");
}

TEST(Why_GateReason)
{
    CHECK(Unchanged("Ina", "Days Passed", 1, Nothing::Gated, "Friend", 1.5f, 3) ==
          "why: Ina | Days Passed x1 | nothing: stopped by the Friend gate (1.5 of 3 days)");
}

TEST(Why_GateDaysAreSafe)
{
    const char* const tail = " of 3 days)";
    const auto line = [&](float days) { return Unchanged("Ina", "d", 1, Nothing::Gated, "Friend", days, 3); };
    const std::string zero = "why: Ina | d x1 | nothing: stopped by the Friend gate (0.0";
    CHECK(line(std::nanf("")) == zero + tail);
    CHECK(line(std::numeric_limits<float>::infinity()) == zero + tail);
    CHECK(line(-std::numeric_limits<float>::infinity()) == zero + tail);
    CHECK(line(-2.5f) == zero + tail);
    CHECK(OneLine(line(std::numeric_limits<float>::max())));
    CHECK(line(std::numeric_limits<float>::max()).find("days)") != std::string::npos);
    CHECK(Unchanged("Ina", "d", 1, Nothing::Gated, "Friend", 0.0f, kMax).find("(0.0 of 2147483647 days)") != std::string::npos);
    CHECK(Unchanged("Ina", "d", 1, Nothing::Gated, "Friend", 0.0f, kMin).find("(0.0 of -2147483648 days)") != std::string::npos);
}

TEST(Why_ScriptAward)
{
    CHECK(Script("Ina", 25, 100, 125, "Stranger") == "why: Ina | script award | +25 | 100 -> 125 Stranger");
    CHECK(Script("Ina", -25, 125, 100, "Stranger") == "why: Ina | script award | -25 | 125 -> 100 Stranger");
}

TEST(Why_PipesAndLineBreaksStayOneLine)
{
    Deed d = Base();
    d.name = "A|b\r\nc";
    d.deed = "x|y\nz";
    d.tier = "T|\r";
    const std::string line = Changed(d);
    CHECK(OneLine(line));
    CHECK(line == "why: A b  c | x y z x2 | rule 3 | like | +6 | 490 -> 496 T  ");
    // The name itself holds no pipe, so the field count is fixed.
    std::size_t bars = 0;
    for (const char c : line) bars += c == '|';
    CHECK(bars == 5);
}

TEST(Why_EveryControlCharacterBecomesASpace)
{
    std::string name = "a";
    for (int c = 1; c < 0x20; ++c) name += static_cast<char>(c);
    name += "\x7F" "\xC2\x85" "\xC2\x9F" "\xE2\x80\xA8" "\xE2\x80\xA9" "b";
    const std::string line = Script(name, 1, 0, 1, "T");
    CHECK(OneLine(line));
    CHECK(line.find("a ") != std::string::npos);
    CHECK(line.find(" b |") != std::string::npos);
    // A control character in the middle of text keeps the text on both sides.
    CHECK(Script(std::string("x\0y", 3), 1, 0, 1, "T").find("x y") != std::string::npos);
}

TEST(Why_LongNamesCutOnACharacterBoundary)
{
    // Euro sign is three bytes: 79 ASCII bytes then a three-byte character would cross byte 80.
    const std::string name = std::string(79, 'a') + "\xE2\x82\xAC" + "tail";
    const std::string line = Script(name, 1, 0, 1, "T");
    CHECK(line == "why: " + std::string(79, 'a') + " | script award | +1 | 0 -> 1 T");
    const std::string exact = std::string(80, 'b');
    CHECK(Script(exact + "c", 1, 0, 1, "T") == "why: " + exact + " | script award | +1 | 0 -> 1 T");
    const std::string two = std::string(78, 'a') + "\xC3\xA9"; // 80 bytes exactly
    CHECK(Script(two, 1, 0, 1, "T").find(two) != std::string::npos);
    const std::string huge(100000, 'z');
    CHECK(Script(huge, 1, 0, 1, huge).size() < 250);
    CHECK(Unchanged(huge, huge, 1, Nothing::Gated, huge, 1.0f, 1).size() < 400);
}

TEST(Why_InvalidUtf8NeverCrashes)
{
    const char* const bad[] = { "\xFF", "\x80", "\xC3", "\xE2\x82", "\xF0\x9F\x98", "\xC0\xAF", "\xED\xA0\x80", "\xF5\x80\x80\x80", "ok\xFFok" };
    for (const char* b : bad) {
        const std::string line = Script(b, 1, 0, 1, b);
        CHECK(OneLine(line));
        CHECK(line.find('?') != std::string::npos);
        CHECK(line.find("script award") != std::string::npos);
    }
    CHECK(Script("ok\xFFok", 1, 0, 1, "T") == "why: ok?ok | script award | +1 | 0 -> 1 T");
    // A valid multibyte name passes through unchanged.
    CHECK(Script("J\xC3\xBCrgen", 1, 0, 1, "T") == "why: J\xC3\xBCrgen | script award | +1 | 0 -> 1 T");
}

TEST(Why_EmptyTextAndTier)
{
    CHECK(Script("", 1, 0, 1, "") == "why:  | script award | +1 | 0 -> 1");
    CHECK(Unchanged("", "", 1, Nothing::NoRule) == "why:  |  x1 | nothing: no rule for this deed");
    CHECK(Unchanged("Ina", "d", 1, Nothing::Gated, "", 1.0f, 2) == "why: Ina | d x1 | nothing: stopped by the gate (1.0 of 2 days)");
}

TEST(Why_ExtremeIntegersPrintTrue)
{
    Deed d = Base();
    d.count = kMin;
    d.rule = kMin;
    d.applied = kMin;
    d.oldPoints = kMax;
    d.newPoints = kMin;
    CHECK(Changed(d) == "why: Ambellina | Dungeons Cleared x-2147483648 | rule -2147483648 | like | -2147483648 | 2147483647 -> -2147483648 Stranger");
    d.count = kMax;
    d.rule = kMax;
    d.applied = kMax;
    d.oldPoints = kMin;
    d.newPoints = kMax;
    CHECK(Changed(d) == "why: Ambellina | Dungeons Cleared x2147483647 | rule 2147483647 | like | +2147483647 | -2147483648 -> 2147483647 Stranger");
    CHECK(Script("n", kMin, 0, kMin, "T") == "why: n | script award | -2147483648 | 0 -> -2147483648 T");
    CHECK(Script("n", kMax, 0, kMax, "T") == "why: n | script award | +2147483647 | 0 -> 2147483647 T");
    CHECK(Unchanged("n", "d", kMax, Nothing::NoRule) == "why: n | d x2147483647 | nothing: no rule for this deed");
}

TEST(Why_ZeroChangeIsPlusZero)
{
    Deed d = Base();
    d.applied = 0;
    d.newPoints = d.oldPoints;
    CHECK(Changed(d).find(" | +0 | ") != std::string::npos);
}

TEST(Why_UnlistedWeightAndPaceAreStable)
{
    Deed d = Base();
    d.weight = 0;
    d.pace = 7;
    CHECK(Changed(d) == "why: Ambellina | Dungeons Cleared x2 | rule 3 | weight 0 | pace 7 | +6 | 490 -> 496 Stranger");
    d.weight = kMin;
    d.pace = kMin;
    CHECK(Changed(d) == "why: Ambellina | Dungeons Cleared x2 | rule 3 | weight -2147483648 | pace -2147483648 | +6 | 490 -> 496 Stranger");
    d.weight = 3;
    d.pace = -2;
    CHECK(Changed(d) == "why: Ambellina | Dungeons Cleared x2 | rule 3 | weight 3 | pace -2 | +6 | 490 -> 496 Stranger");
}

TEST(Why_UnlistedReasonIsStable)
{
    CHECK(Unchanged("n", "d", 1, static_cast<Nothing>(200)) == "why: n | d x1 | nothing: unknown reason");
}

TEST(Why_NegativeRuleAndWeightFollowTheFormula)
{
    // count 2 * rule -3 * weight -2 = +12 on standard pace.
    Deed d = Base();
    d.rule = -3;
    d.weight = -2;
    d.applied = 12;
    d.oldPoints = 10;
    d.newPoints = 22;
    CHECK(Changed(d) == "why: Ambellina | Dungeons Cleared x2 | rule -3 | pet peeve x2 | +12 | 10 -> 22 Stranger");
}

TEST(Why_NegativeZeroDaysPrintAsZero)
{
    CHECK(Unchanged("Ina", "d", 1, Nothing::Gated, "Friend", -0.0f, 3) == "why: Ina | d x1 | nothing: stopped by the Friend gate (0.0 of 3 days)");
}

TEST(Why_ReplacedRunsFillTheBudget)
{
    // Each U+2028 is three bytes in and one space out: 80 of them fill 80 bytes.
    std::string name;
    for (int i = 0; i < 100; ++i) name += "\xE2\x80\xA8";
    CHECK(Script(name, 1, 0, 1, "T") == "why: " + std::string(80, ' ') + " | script award | +1 | 0 -> 1 T");
    // 78 spaces from controls, then a two-byte letter fits exactly; the next one would not.
    const std::string mixed = std::string(78, '\x01') + "\xC3\xA9" + "\xC3\xA9";
    CHECK(Script(mixed, 1, 0, 1, "T") == "why: " + std::string(78, ' ') + "\xC3\xA9 | script award | +1 | 0 -> 1 T");
}

TEST(Why_PaceShownOnlyForGains)
{
    CHECK(PaceShown(-1, 6) == -1);
    CHECK(PaceShown(1, 6) == 1);
    CHECK(PaceShown(0, 6) == 0);
    CHECK(PaceShown(-1, -6) == 0);
    CHECK(PaceShown(1, -6) == 0);
    CHECK(PaceShown(1, 0) == 0);
}

namespace
{
    // A deed that changed the total: every fact says "go on".
    Outcome Moved()
    {
        return { .feeling = true, .points = 6, .following = true, .paced = 6, .refused = false, .applied = 6 };
    }
}

TEST(Why_ReasonEachCase)
{
    CHECK(!Reason(Moved()).has_value());

    auto o = Moved();
    o.feeling = false;
    CHECK(Reason(o) == Nothing::NoFeeling);

    o = Moved();
    o.points = 0;
    CHECK(Reason(o) == Nothing::NoRule);

    o = Moved();
    o.following = false;
    CHECK(Reason(o) == Nothing::NotFollowing);

    o = Moved();
    o.paced = 0;
    CHECK(Reason(o) == Nothing::RoundedToZero);

    o = Moved();
    o.refused = true;
    o.applied = 0;
    CHECK(Reason(o) == Nothing::Gated);

    // A loss on a bond already at 0: allowed, but the total cannot move.
    o = { .feeling = true, .points = -4, .following = true, .paced = -4, .refused = false, .applied = 0 };
    CHECK(Reason(o) == Nothing::AtLimit);

    // A loss that moved is a change.
    o.applied = -4;
    CHECK(!Reason(o).has_value());
}

TEST(Why_ReasonPrecedence)
{
    // Everything wrong at once: each reason wins over the ones after it.
    Outcome o{ .feeling = false, .points = 0, .following = false, .paced = 0, .refused = true, .applied = 0 };
    CHECK(Reason(o) == Nothing::NoFeeling);
    o.feeling = true;
    CHECK(Reason(o) == Nothing::NoRule);
    o.points = 3;
    CHECK(Reason(o) == Nothing::NotFollowing);
    o.following = true;
    CHECK(Reason(o) == Nothing::RoundedToZero);
    o.paced = 3;
    CHECK(Reason(o) == Nothing::Gated);
    o.refused = false;
    CHECK(Reason(o) == Nothing::AtLimit);
    o.applied = 3;
    CHECK(!Reason(o).has_value());
}

TEST(Why_AtLimitAndScriptUnchanged)
{
    CHECK(Unchanged("Ina", "Murders", 1, Nothing::AtLimit) == "why: Ina | Murders x1 | nothing: already at the limit");
    CHECK(ScriptUnchanged("Ina", Nothing::NotFollowing) == "why: Ina | script award | nothing: not following");
    CHECK(ScriptUnchanged("Ina", Nothing::AtLimit) == "why: Ina | script award | nothing: already at the limit");
    CHECK(ScriptUnchanged("I|na\n", Nothing::Gated, "Friend", 1.5f, 3) ==
          "why: I na  | script award | nothing: stopped by the Friend gate (1.5 of 3 days)");
}

TEST(Why_Moment)
{
    CHECK(Moment("Ina", 5, 995, 1000, "Friend") == "why: Ina | shared moment | +5 | 995 -> 1000 Friend");
    CHECK(Moment("Ina", 10, 100, 110, "Stranger") == "why: Ina | shared moment | +10 | 100 -> 110 Stranger");
    CHECK(Moment("I|na\n", 5, 0, 5, "T|x") == "why: I na  | shared moment | +5 | 0 -> 5 T x");
    // A moment only ever gives: the line never shows a minus sign.
    CHECK(OneLine(Moment("a\nb", 5, 0, 5, "T")) && OneLine(MomentNothing("a\nb", "c\rd")));
}

TEST(Why_MomentNothing)
{
    CHECK(MomentNothing("Ina", "reserved") == "why: Ina | shared moment | nothing: reserved");
    CHECK(MomentNothing("Ina", "already counted today") == "why: Ina | shared moment | nothing: already counted today");
    CHECK(MomentNothing("I|na\n", "too|soon") == "why: I na  | shared moment | nothing: too soon");
    // A gain Slow burn's gate held names the gate, in the words the other nothing lines use.
    CHECK(MomentNothing("Ina", NothingText(Nothing::Gated, "Friend", 1.5f, 3)) ==
          "why: Ina | shared moment | nothing: stopped by the Friend gate (1.5 of 3 days)");
    CHECK(NothingText(Nothing::NotFollowing) == "not following");
}

TEST(Why_SceneNothing)
{
    CHECK(SceneNothing("the scene was lost") == "why: scene | shared moment | nothing: the scene was lost");
    CHECK(SceneNothing("no switched-on bond in it") == "why: scene | shared moment | nothing: no switched-on bond in it");
    CHECK(OneLine(SceneNothing("a\nb|c")));
}

TEST(Why_CleanTextCutsAtItsLimit)
{
    CHECK(CleanText("a|b\nc") == "a b c");
    CHECK(CleanText(std::string(200, 'x')) == std::string(80, 'x'));
    CHECK(CleanText(std::string(500, 'x'), 300) == std::string(300, 'x'));
    // A two-byte letter that would cross the limit is left out whole.
    CHECK(CleanText("ab\xC3\xA9", 3) == "ab");
    CHECK(CleanText("ok\xFF", 10) == "ok?");
    CHECK(CleanText("", 10).empty());
}
