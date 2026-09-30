#include "check.h"

#include "ui/screens/Notice.h"

using namespace romantasy::ui;

TEST(notice_warning_tone_matches_failure_messages)
{
    CHECK(IsWarningMessage("That companion could not be added."));
    CHECK(IsWarningMessage("That bond cannot be reset here."));
    CHECK(IsWarningMessage("That personality is protected."));
    CHECK(IsWarningMessage("That bond is protected."));
    CHECK(!IsWarningMessage("A new bond was entered in the ledger."));
    CHECK(!IsWarningMessage("Companion personality sealed."));
    CHECK(!IsWarningMessage("Player-created bond reset."));
    CHECK(!IsWarningMessage("Ledger refreshed."));
    CHECK(!IsWarningMessage(""));
}

TEST(notice_shows_once_per_revision_when_armed)
{
    NoticeState notice;
    UiState state;
    state.message = "Romance records synchronized.";
    state.revision = 1;
    UpdateNotice(notice, state, 0.016f);
    CHECK(notice.phase == NoticeState::Phase::Hidden);  // never armed

    notice.Arm(state.revision);
    UpdateNotice(notice, state, 0.016f);
    CHECK(notice.phase == NoticeState::Phase::Hidden);  // nothing new since arming

    state.revision = 2;
    state.message = "Player-created bond removed.";
    UpdateNotice(notice, state, 0.016f);
    CHECK(notice.phase == NoticeState::Phase::In);
    CHECK(notice.text == "Player-created bond removed.");
    CHECK(!notice.warn);
    CHECK(!notice.armed);

    for (int i = 0; i < 20; ++i) UpdateNotice(notice, state, 0.016f);  // 0.32 s > the 0.2 s fade in
    CHECK(notice.phase == NoticeState::Phase::Hold);
    CHECK_NEAR(NoticeProgress(notice), 1.0f, 1e-6);

    state.revision = 3;  // an unarmed push changes nothing
    UpdateNotice(notice, state, 0.016f);
    CHECK(notice.phase == NoticeState::Phase::Hold);

    for (int i = 0; i < 250; ++i) UpdateNotice(notice, state, 0.016f);  // 4 s > 3.4 hold + 0.2 out
    CHECK(notice.phase == NoticeState::Phase::Hidden);
    CHECK(notice.text.empty());
    CHECK_NEAR(NoticeProgress(notice), 0.0f, 1e-6);
}

TEST(notice_warning_message_sets_warn_tone)
{
    NoticeState notice;
    UiState state;
    state.revision = 5;
    notice.Arm(4);
    state.message = "That bond is protected.";
    UpdateNotice(notice, state, 0.016f);
    CHECK(notice.phase == NoticeState::Phase::In);
    CHECK(notice.warn);
}
