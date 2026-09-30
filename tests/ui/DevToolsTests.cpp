#include "check.h"

#include "ui/screens/DevTools.h"

#include <string>

using namespace romantasy::ui;

TEST(debug_buttons_carry_expected_labels_and_requests)
{
    CHECK(DebugButtonCount == 5);
    CHECK(std::string(DebugButtonAt(0).label) == "Bond +500");
    CHECK(DebugButtonAt(0).kind == widgets::ButtonKind::Primary);
    CHECK(std::string(DebugButtonAt(1).label) == "Bond -500");
    CHECK(DebugButtonAt(1).kind == widgets::ButtonKind::Danger);
    CHECK(std::string(DebugButtonAt(2).label) == "Side quest +1");
    CHECK(DebugButtonAt(2).kind == widgets::ButtonKind::Secondary);
    CHECK(std::string(DebugButtonAt(3).label) == "Murder +1");
    CHECK(DebugButtonAt(3).kind == widgets::ButtonKind::Danger);
    CHECK(std::string(DebugButtonAt(4).label) == "Refresh");
    CHECK(DebugButtonAt(4).kind == widgets::ButtonKind::Ghost);
    CHECK(DebugButtonAt(4).refresh);

    const auto plus = DebugRequestFor(0);
    CHECK(plus.has_value() && plus->points.has_value() && *plus->points == 500);
    const auto minus = DebugRequestFor(1);
    CHECK(minus.has_value() && minus->points.has_value() && *minus->points == -500);
    const auto quest = DebugRequestFor(2);
    CHECK(quest.has_value() && !quest->points.has_value() && quest->stat == "Side Quests Completed" && quest->delta == 1);
    const auto murder = DebugRequestFor(3);
    CHECK(murder.has_value() && murder->stat == "Murders" && murder->delta == 1);
    CHECK(!DebugRequestFor(4).has_value());
    CHECK(!DebugRequestFor(-1).has_value());
    CHECK(!DebugRequestFor(5).has_value());
}
