#include "check.h"
#include "conditions/RomanceConditionQuery.h"

#include <optional>
#include <string_view>

using namespace romantasy::conditions;

TEST(condition_queries_recognize_only_reserved_names)
{
    CHECK(ParseQuery("Romantasy_Registered") == Query::Registered);
    CHECK(ParseQuery("Romantasy_Level") == Query::Level);
    CHECK(ParseQuery("Romantasy_Points") == Query::Points);
    CHECK(ParseQuery("Romantasy_LevelExtra") == Query::None);
    CHECK(ParseQuery("romantasy_level") == Query::None);
    CHECK(ParseQuery("IsStaggering") == Query::None);
    CHECK(ParseQuery("") == Query::None);
}

TEST(condition_queries_keep_actor_results_independent)
{
    CHECK(Evaluate(Query::Points, 750) == 750.0);
    CHECK(Evaluate(Query::Level, 750) == 2.0);
    CHECK(Evaluate(Query::Points, 2125) == 2125.0);
    CHECK(Evaluate(Query::Level, 2125) == 5.0);
    CHECK(Evaluate(Query::Registered, 0) == 1.0);
    CHECK(Evaluate(Query::Level, 0) == 1.0);
}

TEST(condition_queries_distinguish_unmanaged_from_zero_points)
{
    CHECK(Evaluate(Query::Registered, std::nullopt) == 0.0);
    CHECK(Evaluate(Query::Level, std::nullopt) == 0.0);
    CHECK(Evaluate(Query::Points, std::nullopt) == -1.0);
    CHECK(!Evaluate(Query::None, 2000).has_value());
}

TEST(condition_level_boundaries_match_public_api)
{
    const int thresholds[] = { 0, 500, 1000, 1500, 2000, 2500 };
    for (int i = 0; i < 6; ++i) {
        CHECK(Evaluate(Query::Level, thresholds[i]) == static_cast<double>(i + 1));
        if (i > 0) {
            CHECK(Evaluate(Query::Level, thresholds[i] - 1) == static_cast<double>(i));
        }
    }
    CHECK(Evaluate(Query::Level, 100000) == 6.0);
}

TEST(condition_mood_query_is_reserved)
{
    CHECK(ParseQuery("Romantasy_Mood") == Query::Mood);
    CHECK(ParseQuery("Romantasy_MoodExtra") == Query::None);
    CHECK(ParseQuery("romantasy_mood") == Query::None);
}

TEST(condition_mood_answers_state_numbers)
{
    using romantasy::mood::State;
    CHECK(EvaluateMood(std::nullopt, std::nullopt) == 0.0);
    CHECK(EvaluateMood(std::nullopt, State::Warm) == 0.0);
    CHECK(EvaluateMood(750, std::nullopt) == 2.0);
    CHECK(EvaluateMood(0, std::nullopt) == 2.0);
    CHECK(EvaluateMood(750, State::Strained) == 1.0);
    CHECK(EvaluateMood(750, State::Settled) == 2.0);
    CHECK(EvaluateMood(750, State::Warm) == 3.0);
}

TEST(condition_old_queries_unchanged_beside_mood)
{
    CHECK(Evaluate(Query::Registered, 750) == 1.0);
    CHECK(Evaluate(Query::Level, 750) == 2.0);
    CHECK(Evaluate(Query::Points, 750) == 750.0);
    CHECK(Evaluate(Query::Registered, std::nullopt) == 0.0);
    CHECK(Evaluate(Query::Level, std::nullopt) == 0.0);
    CHECK(Evaluate(Query::Points, std::nullopt) == -1.0);
}
