#include "check.h"

#include "romance/Journal.h"
#include "ui/JournalJson.h"

#include <limits>
#include <string>

using namespace romantasy;
using nlohmann::json;
using journal::Journal;
using journal::MilestoneKind;

namespace
{
    Journal SmallJournal()
    {
        Journal j;
        j.milestones.push_back({ MilestoneKind::Began, 1, 2.9f });
        j.milestones.push_back({ MilestoneKind::Reached, 3, 10.0f });
        j.milestones.push_back({ MilestoneKind::Fell, 2, 12.5f });
        j.days.push_back({ 2, 40, 0 });
        j.days.push_back({ 10, 25, 5 });
        j.gains.push_back({ "Cleared a dungeon", 30, 10.4f });
        j.losses.push_back({ "Stole something", -12, 12.5f });
        j.history.push_back({ "Stole something", -12, 12.5f });
        j.history.push_back({ "Cleared a dungeon", 30, 10.4f });
        return j;
    }
}

TEST(JournalJson_Full)
{
    const Journal j = SmallJournal();
    const json out = ui::JournalJson("00ABCDEF", &j, 3.14159f);
    CHECK(out["identity"] == "00ABCDEF");
    CHECK(out["found"] == true);
    CHECK(out["began"] == 3);
    CHECK(out["daysTogether"].is_number());
    CHECK_NEAR(out["daysTogether"].get<double>(), 3.1, 1e-9);

    const auto& m = out["milestones"];
    CHECK(m.size() == 3);
    CHECK(m[0]["kind"] == "began" && m[0]["level"] == 1 && m[0]["day"] == 3);
    CHECK(m[1]["kind"] == "reached" && m[1]["level"] == 3 && m[1]["day"] == 11);
    CHECK(m[2]["kind"] == "fell" && m[2]["level"] == 2 && m[2]["day"] == 13);

    const auto& d = out["days"];
    CHECK(d.size() == 2);
    CHECK(d[0]["day"] == 3 && d[0]["gained"] == 40 && d[0]["lost"] == 0);
    CHECK(d[1]["day"] == 11 && d[1]["gained"] == 25 && d[1]["lost"] == 5);

    CHECK(out["gains"].size() == 1);
    CHECK(out["gains"][0]["label"] == "Cleared a dungeon");
    CHECK(out["gains"][0]["points"] == 30 && out["gains"][0]["day"] == 11);
    CHECK(out["losses"].size() == 1);
    CHECK(out["losses"][0]["label"] == "Stole something");
    CHECK(out["losses"][0]["points"] == -12 && out["losses"][0]["day"] == 13);

    CHECK(out["history"].size() == 2);
    CHECK(out["history"][0]["label"] == "Stole something");
    CHECK(out["history"][1]["label"] == "Cleared a dungeon");
}

TEST(JournalJson_NotFound)
{
    const json out = ui::JournalJson("1A", nullptr, 5.0f);
    CHECK(out.size() == 2);
    CHECK(out["identity"] == "1A");
    CHECK(out["found"] == false);
}

TEST(JournalJson_Empty)
{
    Journal j;
    const json out = ui::JournalJson("1A", &j, 0.0f);
    CHECK(out["found"] == true);
    CHECK(out["began"] == 0);
    CHECK(out["daysTogether"] == 0);
    for (const char* key : { "milestones", "days", "gains", "losses", "history" }) {
        CHECK(out[key].is_array() && out[key].empty());
    }
}

TEST(JournalJson_BadDaysTogetherIsZero)
{
    Journal j;
    CHECK(ui::JournalJson("1", &j, -4.0f)["daysTogether"] == 0);
    CHECK(ui::JournalJson("1", &j, std::numeric_limits<float>::quiet_NaN())["daysTogether"] == 0);
    CHECK(ui::JournalJson("1", &j, std::numeric_limits<float>::infinity())["daysTogether"] == 0);
}

TEST(JournalJson_UnknownMilestoneKindLeftOut)
{
    Journal j;
    j.milestones.push_back({ MilestoneKind::Began, 1, 0.0f });
    j.milestones.push_back({ static_cast<MilestoneKind>(9), 1, 1.0f });
    j.milestones.push_back({ MilestoneKind::Reached, 2, 2.0f });
    const json out = ui::JournalJson("1", &j, 1.0f);
    CHECK(out["milestones"].size() == 2);
    CHECK(out["milestones"][1]["kind"] == "reached");
}

TEST(JournalJson_InvalidUtf8LabelDumps)
{
    Journal j;
    j.history.push_back({ std::string("bad \xFF byte"), 5, 1.0f });
    const json out = ui::JournalJson("1", &j, 1.0f);
    bool threw = false;
    std::string text;
    try {
        text = out.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    } catch (...) {
        threw = true;
    }
    CHECK(!threw);
    CHECK(text.find("bad ") != std::string::npos);
}

namespace
{
    std::vector<std::string> Kinds(const json& out)
    {
        std::vector<std::string> kinds;
        for (const auto& m : out["milestones"]) {
            kinds.push_back(m["kind"].get<std::string>());
        }
        return kinds;
    }
}

TEST(Journal_SharedMilestoneInDayOrder)
{
    const Journal j = SmallJournal(); // began day 3, reached day 11, fell day 13

    // Day 5: before the later reached milestone.
    const json before = ui::JournalJson("1", &j, 1.0f, 4.5f);
    CHECK(Kinds(before) == std::vector<std::string>({ "began", "shared", "reached", "fell" }));
    CHECK(before["milestones"][1]["level"] == 0 && before["milestones"][1]["day"] == 5);

    // Day 11, the same day as reached: after it.
    const json same = ui::JournalJson("1", &j, 1.0f, 10.75f);
    CHECK(Kinds(same) == std::vector<std::string>({ "began", "reached", "shared", "fell" }));
    CHECK(same["milestones"][2]["day"] == 11);

    // Day 20: at the end.
    const json after = ui::JournalJson("1", &j, 1.0f, 19.0f);
    CHECK(Kinds(after) == std::vector<std::string>({ "began", "reached", "fell", "shared" }));
    CHECK(after["milestones"][3]["day"] == 20);

    // Day 1 (game day 0): before every later milestone.
    CHECK(Kinds(ui::JournalJson("1", &j, 1.0f, 0.0f)) == std::vector<std::string>({ "shared", "began", "reached", "fell" }));

    // A journal with no milestones yet still gets it.
    Journal empty;
    const json lone = ui::JournalJson("1", &empty, 1.0f, 2.0f);
    CHECK(Kinds(lone) == std::vector<std::string>({ "shared" }));
}

TEST(Journal_NoSharedWhenNever)
{
    const Journal j = SmallJournal();
    CHECK(ui::JournalJson("1", &j, 1.0f)["milestones"].size() == 3);
    CHECK(ui::JournalJson("1", &j, 1.0f, -1.0f)["milestones"].size() == 3);
    CHECK(ui::JournalJson("1", &j, 1.0f, -0.5f)["milestones"].size() == 3);
    CHECK(ui::JournalJson("1", &j, 1.0f, std::numeric_limits<float>::quiet_NaN())["milestones"].size() == 3);
    CHECK(ui::JournalJson("1", &j, 1.0f, std::numeric_limits<float>::infinity())["milestones"].size() == 3);
}

TEST(Journal_NoSharedWhenNoJournal)
{
    const json out = ui::JournalJson("1A", nullptr, 5.0f, 4.0f);
    CHECK(out.size() == 2);
    CHECK(out["found"] == false);
    CHECK(!out.contains("milestones"));
}
