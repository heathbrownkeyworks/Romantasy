#include "check.h"

#include "ui/screens/Tiers.h"
#include "ui/screens/Theme.h"
#include "ui/screens/UiState.h"

using namespace romantasy::ui;

TEST(tier_index_follows_thresholds)
{
    CHECK(tiers::TierIndex(-5) == 0);
    CHECK(tiers::TierIndex(0) == 0);
    CHECK(tiers::TierIndex(499) == 0);
    CHECK(tiers::TierIndex(500) == 1);
    CHECK(tiers::TierIndex(1240) == 2);
    CHECK(tiers::TierIndex(2499) == 4);
    CHECK(tiers::TierIndex(2500) == 5);
    CHECK(tiers::TierIndex(9999) == 5);
}

TEST(tier_next_threshold_and_until_next)
{
    CHECK(tiers::NextThreshold(0) == 500);
    CHECK(tiers::NextThreshold(1240) == 1500);
    CHECK(tiers::NextThreshold(2240) == 2500);
    CHECK(tiers::NextThreshold(2500) == -1);
    CHECK(tiers::UntilNext(2240) == 260);
    CHECK(tiers::UntilNext(0) == 500);
    CHECK(tiers::UntilNext(2630) == 0);
}

TEST(tier_meter_fraction_spans_the_whole_ladder)
{
    CHECK_NEAR(tiers::MeterFraction(0), 0.0f, 1e-6);
    CHECK_NEAR(tiers::MeterFraction(1250), 0.5f, 1e-6);
    CHECK_NEAR(tiers::MeterFraction(2500), 1.0f, 1e-6);
    CHECK_NEAR(tiers::MeterFraction(4000), 1.0f, 1e-6);
    CHECK_NEAR(tiers::MeterFraction(-1), 0.0f, 1e-6);
}

TEST(tier_names_match_the_ladder)
{
    CHECK(std::string(tiers::Names[0]) == "Stranger");
    CHECK(std::string(tiers::Names[2]) == "Friend");
    CHECK(std::string(tiers::Names[5]) == "Spouse");
    CHECK(tiers::Thresholds[3] == 1500);
}

TEST(count_bonds_respects_away_visibility)
{
    UiState state;
    FollowerRow a; a.isFollowing = true;
    FollowerRow b; b.isFollowing = false;
    FollowerRow c; c.isFollowing = true;
    state.followers = { a, b, c };
    const auto all = tiers::CountBonds(state, true);
    CHECK(all.bonds == 3 && all.following == 2);
    const auto onlyFollowing = tiers::CountBonds(state, false);
    CHECK(onlyFollowing.bonds == 2 && onlyFollowing.following == 2);
}

TEST(lerp_color_hits_both_endpoints_and_midpoint)
{
    CHECK(theme::LerpColor(theme::Fg1, theme::Amber, 0.0f) == theme::Fg1);
    CHECK(theme::LerpColor(theme::Fg1, theme::Amber, 1.0f) == theme::Amber);
    const ImU32 mid = theme::LerpColor(IM_COL32(0, 0, 0, 255), IM_COL32(200, 100, 50, 255), 0.5f);
    CHECK(((mid >> IM_COL32_R_SHIFT) & 0xFF) == 100);
    CHECK(((mid >> IM_COL32_G_SHIFT) & 0xFF) == 50);
    CHECK(((mid >> IM_COL32_B_SHIFT) & 0xFF) == 25);
    CHECK(((mid >> IM_COL32_A_SHIFT) & 0xFF) == 255);
}
