#include "check.h"
#include "ostim/Moments.h"

#include <limits>
#include <string>

using namespace romantasy::ostim;

namespace
{
    constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
    constexpr float kInf = std::numeric_limits<float>::infinity();

    MomentFacts Baseline()
    {
        MomentFacts f;
        f.participants = 2;
        f.playerPresent = true;
        f.bonded = 1;
        f.kind = Kind::Affection;
        f.seconds = 60.0f;
        f.level = 3;
        return f;
    }

    bool Is(const Verdict& v, bool counted, std::int32_t points, Reason reason)
    {
        return v.counted == counted && v.points == points && v.reason == reason;
    }

    StartFacts StartBaseline()
    {
        StartFacts f;
        f.canStart = true;
        f.bonded = true;
        f.level = 3;
        f.mood = 2;
        return f;
    }
}

TEST(Moment_KindOfAction)
{
    for (const auto& id : kAffectionActions) CHECK(KindOfAction(id) == Kind::Affection);
    for (const auto& id : kClosenessActions) CHECK(KindOfAction(id) == Kind::Closeness);
    CHECK(KindOfAction("vaginalsex") == Kind::Intimacy);
    CHECK(KindOfAction("choking") == Kind::Intimacy);
    CHECK(KindOfAction("") == Kind::Intimacy);
    CHECK(KindOfAction("KISSING") == Kind::Closeness);
    CHECK(Deeper(Kind::Affection, Kind::Intimacy) == Kind::Intimacy);
    CHECK(Deeper(Kind::Closeness, Kind::None) == Kind::Closeness);
}

TEST(Moment_ActionCSV)
{
    CHECK(ActionCSV(Kind::Affection) == "holdinghand,hugging,kissingcheek,kissinghand,pattinghead,strokinghead,dancing");
    const std::string close = ActionCSV(Kind::Closeness);
    std::size_t commas = 0;
    for (const char c : close) commas += c == ',' ? 1 : 0;
    CHECK(commas == 13);
    CHECK(close.starts_with(ActionCSV(Kind::Affection) + ",kissing,"));
    CHECK(ActionCSV(Kind::Intimacy).empty());
    CHECK(ActionCSV(Kind::None).empty());
}

TEST(Moment_ForceTags)
{
    CHECK(IsForceActorTag("dominant"));
    CHECK(IsForceActorTag("aggressor"));
    CHECK(IsForceActorTag("victim"));
    CHECK(IsForceActorTag("submissive"));
    CHECK(IsForceActorTag("-dominant"));
    CHECK(IsForceActorTag("DOMINANT"));
    CHECK(!IsForceActorTag("romantic"));
    CHECK(!IsForceActorTag("--dominant"));
    CHECK(!IsForceActorTag(""));
    CHECK(kForceSceneTag == "aggressive");
}

TEST(Moment_JudgeOrder)
{
    CHECK(Is(Judge(Baseline()), true, 5, Reason::None));

    MomentFacts f = Baseline();
    f.stoppedForMismatch = true;
    CHECK(Judge(f).reason == Reason::NotAsked);
    f.enabled = false;
    CHECK(Judge(f).reason == Reason::NotAsked);

    f = Baseline();
    f.enabled = false;
    CHECK(Is(Judge(f), false, 0, Reason::Off));
    f.participants = 3;
    CHECK(Judge(f).reason == Reason::Off);

    f = Baseline();
    f.participants = 3;
    CHECK(Is(Judge(f), false, 0, Reason::NotTwo));
    f.force = true;
    CHECK(Judge(f).reason == Reason::NotTwo);
    f = Baseline();
    f.playerPresent = false;
    CHECK(Judge(f).reason == Reason::NotTwo);
    f = Baseline();
    f.bonded = 2;
    CHECK(Judge(f).reason == Reason::NotTwo);
    f.bonded = 0;
    CHECK(Judge(f).reason == Reason::NotTwo);

    f = Baseline();
    f.force = true;
    CHECK(Is(Judge(f), false, 0, Reason::NotGentle));
    f.kind = Kind::None;
    CHECK(Judge(f).reason == Reason::NotGentle);

    f = Baseline();
    f.kind = Kind::None;
    CHECK(Is(Judge(f), false, 0, Reason::NothingHappened));
    f.seconds = 1.0f;
    CHECK(Judge(f).reason == Reason::NothingHappened);

    f = Baseline();
    f.seconds = 5.0f;
    CHECK(Is(Judge(f), false, 0, Reason::TooShort));
    f.level = 0;
    CHECK(Judge(f).reason == Reason::TooShort);
    f = Baseline();
    f.seconds = kNaN;
    CHECK(Judge(f).reason == Reason::TooShort);
    f.seconds = kInf;
    CHECK(Judge(f).reason == Reason::TooShort);

    f = Baseline();
    f.level = 2;
    CHECK(Is(Judge(f), false, 0, Reason::TooSoon));

    f.welcome.feeling = Feeling::Reserved;
    CHECK(Judge(f).reason == Reason::TooSoon);
    f.welcome.feeling = Feeling::Warm;
    f.lastPointsDay = 4.2f;
    f.today = 4.9f;
    CHECK(Judge(f).reason == Reason::TooSoon);

    f = Baseline();
    f.welcome.feeling = Feeling::Reserved;
    f.lastPointsDay = 4.2f;
    f.today = 4.9f;
    CHECK(Is(Judge(f), true, 0, Reason::Reserved));
}

TEST(Moment_JudgeCounts)
{
    MomentFacts f = Baseline();
    f.welcome.feeling = Feeling::Treasured;
    CHECK(Is(Judge(f), true, 10, Reason::None));

    f.welcome.feeling = Feeling::Reserved;
    CHECK(Is(Judge(f), true, 0, Reason::Reserved));

    f = Baseline();
    f.lastPointsDay = 4.2f;
    f.today = 4.9f;
    CHECK(Is(Judge(f), true, 0, Reason::AlreadyToday));
    f.today = 5.0f;
    CHECK(Is(Judge(f), true, 5, Reason::None));
    f.lastPointsDay = 6.0f;
    f.today = 5.0f;
    CHECK(Is(Judge(f), true, 5, Reason::None));
    f.lastPointsDay = 5.9f;
    f.today = 5.1f;
    CHECK(Is(Judge(f), true, 5, Reason::None));
    f.lastPointsDay = 0.0f;
    f.today = 0.5f;
    CHECK(Is(Judge(f), true, 0, Reason::AlreadyToday));

    f = Baseline();
    f.lastPointsDay = kNaN;
    f.today = 5.0f;
    CHECK(Judge(f).points == 5);
    f.lastPointsDay = 5.0f;
    f.today = kNaN;
    CHECK(Judge(f).points == 5);
    f.today = kInf;
    CHECK(Judge(f).points == 5);
    f.lastPointsDay = kInf;
    f.today = kInf;
    CHECK(Judge(f).points == 5);
    f.lastPointsDay = -1.0f;
    f.today = 0.5f;
    CHECK(Judge(f).points == 5);
}

TEST(Moment_JudgeTiers)
{
    MomentFacts f = Baseline();
    f.kind = Kind::Closeness;
    f.level = 3;
    CHECK(Judge(f).reason == Reason::TooSoon);
    f.level = 4;
    CHECK(Judge(f).counted);

    f.kind = Kind::Intimacy;
    CHECK(Judge(f).reason == Reason::TooSoon);
    f.level = 5;
    CHECK(Judge(f).counted);

    f = Baseline();
    f.welcome.affection = 0;
    f.level = 6;
    CHECK(Judge(f).reason == Reason::TooSoon);
}

TEST(Moment_JudgeBoundary)
{
    MomentFacts f = Baseline();
    f.seconds = 19.99f;
    CHECK(Judge(f).reason == Reason::TooShort);
    f.seconds = 20.0f;
    CHECK(Judge(f).counted);
}

TEST(Moment_Noted)
{
    MomentState s;
    Noted(s, Verdict{ true, 5, Reason::None }, 4.5f);
    CHECK(s.count == 1);
    CHECK(s.firstDay == 4.5f && s.lastDay == 4.5f && s.lastPointsDay == 4.5f);

    Noted(s, Verdict{ true, 0, Reason::AlreadyToday }, 4.8f);
    CHECK(s.count == 2);
    CHECK(s.firstDay == 4.5f && s.lastDay == 4.8f && s.lastPointsDay == 4.5f);

    const MomentState before = s;
    Noted(s, Verdict{ false, 0, Reason::TooShort }, 9.0f);
    CHECK(s == before);

    const MomentState beforeBadDay = s;
    Noted(s, Verdict{ true, 5, Reason::None }, kNaN);
    CHECK(s.count == beforeBadDay.count + 1);
    CHECK(s.firstDay == beforeBadDay.firstDay && s.lastDay == beforeBadDay.lastDay && s.lastPointsDay == beforeBadDay.lastPointsDay);
    Noted(s, Verdict{ true, 5, Reason::None }, kInf);
    CHECK(s.count == beforeBadDay.count + 2);
    CHECK(s.lastDay == beforeBadDay.lastDay && s.lastPointsDay == beforeBadDay.lastPointsDay);

    s.count = std::numeric_limits<std::uint32_t>::max();
    Noted(s, Verdict{ true, 5, Reason::None }, 6.0f);
    CHECK(s.count == std::numeric_limits<std::uint32_t>::max());
}

TEST(Moment_DaysSince)
{
    MomentState s;
    CHECK(DaysSince(s, 5.5f) == -1.0f);
    s.lastDay = 3.0f;
    CHECK(DaysSince(s, 5.5f) == 2.5f);
    CHECK(DaysSince(s, 2.0f) == 0.0f);
    CHECK(DaysSince(s, kNaN) == 0.0f);
    CHECK(DaysSince(s, kInf) == 0.0f);
    s.lastDay = kNaN;
    CHECK(DaysSince(s, 5.0f) == -1.0f);
    s.lastDay = kInf;
    CHECK(DaysSince(s, 5.0f) == -1.0f);
}

TEST(Moment_CheckStartOrder)
{
    StartFacts f = StartBaseline();
    CHECK(CheckStart(f).code == StartCode::Accepted);
    CHECK(CheckStart(f).kind == Kind::Affection);

    f = StartBaseline();
    f.canStart = false;
    f.bonded = false;
    CHECK(CheckStart(f).code == StartCode::NotAvailable);
    f = StartBaseline();
    f.kindKnown = false;
    CHECK(CheckStart(f).code == StartCode::NotAvailable);

    f = StartBaseline();
    f.bonded = false;
    f.level = 0;
    CHECK(CheckStart(f).code == StartCode::NotABond);

    f = StartBaseline();
    f.level = 2;
    f.mood = 1;
    CHECK(CheckStart(f).code == StartCode::TooSoon);
    CHECK(CheckStart(f).kind == Kind::None);

    f = StartBaseline();
    f.mood = 1;
    f.inCombat = true;
    CHECK(CheckStart(f).code == StartCode::Strained);

    f = StartBaseline();
    f.inCombat = true;
    CHECK(CheckStart(f).code == StartCode::NotNow);
    f = StartBaseline();
    f.inScene = true;
    CHECK(CheckStart(f).code == StartCode::NotNow);
    f = StartBaseline();
    f.pending = true;
    CHECK(CheckStart(f).code == StartCode::NotNow);

    f = StartBaseline();
    f.level = 4;
    CHECK(CheckStart(f).code == StartCode::Accepted);
    CHECK(CheckStart(f).kind == Kind::Closeness);

    f.asked = Kind::Intimacy;
    CHECK(CheckStart(f).code == StartCode::TooSoon);
    CHECK(CheckStart(f).kind == Kind::Intimacy);
    f.asked = Kind::Affection;
    CHECK(CheckStart(f).code == StartCode::Accepted);
    CHECK(CheckStart(f).kind == Kind::Affection);
}

TEST(Moment_NodeFits)
{
    CHECK(NodeFits(Kind::Affection, Kind::None));
    CHECK(NodeFits(Kind::Affection, Kind::Affection));
    CHECK(!NodeFits(Kind::Affection, Kind::Closeness));
    CHECK(NodeFits(Kind::Closeness, Kind::Affection));
    CHECK(!NodeFits(Kind::Closeness, Kind::Intimacy));
}

TEST(Moment_ParseKey)
{
    for (const Kind k : { Kind::Affection, Kind::Closeness, Kind::Intimacy }) {
        const auto parsed = ParseKind(Key(k));
        CHECK(parsed.has_value() && *parsed == k);
        CHECK(!Label(k).empty());
    }
    CHECK(Label(Kind::Affection) == "A tender moment");
    CHECK(Label(Kind::Closeness) == "A quiet hour together");
    CHECK(Label(Kind::Intimacy) == "Time alone together");
    CHECK(!ParseKind("Affection").has_value());
    CHECK(!ParseKind("").has_value());
    CHECK(Key(Kind::None).empty() && Label(Kind::None).empty());
    CHECK(Text(Reason::AlreadyToday) == "already counted today" && Text(Reason::None).empty());
}
