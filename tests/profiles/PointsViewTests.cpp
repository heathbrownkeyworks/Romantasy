#include "check.h"
#include "romance/PointsView.h"

using romantasy::PointsView;

namespace
{
    constexpr std::uint32_t kReference = 0x0001A2B3;
    constexpr std::uint32_t kOtherReference = 0x0001A2B4;
    constexpr std::uint32_t kBase = 0x00013BAB;
}

TEST(PointsView_FindHitsOnlyExactReference)
{
    PointsView view;
    view.byReference[kReference] = 750;
    CHECK(view.Find(kReference) == 750);
    CHECK(!view.Find(kOtherReference));
    CHECK(!view.Find(0));
}

TEST(PointsView_DisabledProfileAnswersNothing)
{
    PointsView view;
    view.profiles[kBase] = { false, 500 };
    view.byReference[kReference] = 750;
    view.saved[kReference] = 300;
    CHECK(!view.Query(kReference, kBase, true));
}

TEST(PointsView_KnownReferenceWins)
{
    PointsView view;
    view.profiles[kBase] = { true, 500 };
    view.byReference[kReference] = 750;
    view.saved[kReference] = 300;
    view.saved[kBase] = 1750;
    CHECK(view.Query(kReference, kBase, true) == 750);
}

TEST(PointsView_ProfileFallsBackToSavedThenStarting)
{
    PointsView view;
    view.profiles[kBase] = { true, 500 };
    view.saved[kReference] = 300;
    CHECK(view.Query(kReference, kBase, true) == 300);
    view.saved.clear();
    CHECK(view.Query(kReference, kBase, true) == 500);
}

TEST(PointsView_NonUniqueBaseIgnoresBaseSave)
{
    PointsView view;
    view.profiles[kBase] = { true, 500 };
    view.saved[kBase] = 1750;
    CHECK(view.Query(kReference, kBase, false) == 500);
    CHECK(view.Query(kReference, kBase, true) == 1750);
}

TEST(PointsView_KnownReferenceWithoutProfileAnswersPoints)
{
    PointsView view;
    view.byReference[kReference] = 750;
    view.saved[kReference] = 300;
    view.saved[kBase] = 1750;
    CHECK(view.Query(kReference, kBase, true) == 750);
    CHECK(view.Query(kReference, kBase, false) == 750);
}

TEST(PointsView_ZeroReferenceWithProfileFallsToBaseSaveOrStarting)
{
    PointsView view;
    view.profiles[kBase] = { true, 500 };
    view.byReference[kReference] = 750;
    view.saved[0] = 999;  // never matched: reference 0 skips the reference lookups
    view.saved[kBase] = 1750;
    CHECK(view.Query(0, kBase, true) == 1750);
    CHECK(view.Query(0, kBase, false) == 500);
    view.saved.erase(kBase);
    CHECK(view.Query(0, kBase, true) == 500);
}

TEST(PointsView_NoProfileNoFollowerAnswersNothing)
{
    PointsView view;
    view.saved[kReference] = 300;
    view.saved[kBase] = 1750;
    view.byReference[kOtherReference] = 750;
    CHECK(!view.Query(kReference, kBase, true));
}

TEST(PointsView_FindMood)
{
    PointsView view;
    const romantasy::mood::Mood warm{ 40, 12.5f };
    view.moodByReference[kReference] = warm;
    view.moodByReference[0] = { -50, 1.0f };  // never matched: reference 0 has no mood
    CHECK(view.FindMood(kReference) == warm);
    CHECK(!view.FindMood(kOtherReference));
    CHECK(!view.FindMood(0));
}
