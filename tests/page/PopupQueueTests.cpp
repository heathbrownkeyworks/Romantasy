#include "check.h"

#include "ui/PopupQueue.h"

using namespace romantasy::ui;
using namespace std::chrono_literals;
using nlohmann::json;
using Head = PopupQueue::Head;

namespace
{
    const PopupQueue::Clock::time_point t0{ 1000s };
}

TEST(Popup_EmptyQueue)
{
    PopupQueue q;
    CHECK(q.HeadState() == Head::None);
    CHECK(q.HeadPayload() == nullptr);
    CHECK(q.Size() == 0);
    CHECK(!q.ShouldAttempt(t0, true));
    q.Dismiss();
    q.MarkRefused();
    q.MarkRequested(t0);
    q.MarkShown(t0);
    CHECK(q.HeadState() == Head::None);
    CHECK(!q.ShownTimedOut(t0 + 1000s));
    CHECK(!q.ShownTooLong(t0 + 1000s));
    CHECK(q.DropExpired(t0 + 1000s) == 0);
}

TEST(Popup_PushAndAttempt)
{
    PopupQueue q;
    q.Push(json{ { "a", 1 } }, t0);
    CHECK(q.HeadState() == Head::Waiting);
    CHECK(q.HeadPayload() != nullptr && (*q.HeadPayload())["a"] == 1);
    CHECK(q.ShouldAttempt(t0, false));
    CHECK(!q.ShouldAttempt(t0 + 500ms, false));
    CHECK(q.ShouldAttempt(t0 + 1s, false));
    CHECK(q.ShouldAttempt(t0 + 1s + 100ms, true));
}

TEST(Popup_RequestedAndShown)
{
    PopupQueue q;
    q.Push(json::object(), t0);
    q.MarkRequested(t0);
    CHECK(q.HeadState() == Head::Requested);
    CHECK(!q.ShouldAttempt(t0 + 5s, true));
    CHECK(!q.ShownTimedOut(t0 + 10s));
    CHECK(q.ShownTimedOut(t0 + 11s));
    q.MarkShown(t0 + 2s);
    CHECK(q.HeadState() == Head::Shown);
    CHECK(!q.ShownTimedOut(t0 + 1000s));
    CHECK(!q.ShownTooLong(t0 + 2s + 30s));
    CHECK(q.ShownTooLong(t0 + 2s + 31s));
}

TEST(Popup_RefusedKeepsWaitClock)
{
    PopupQueue q;
    q.Push(json::object(), t0);
    q.MarkRequested(t0 + 100s);
    q.MarkRefused();
    CHECK(q.HeadState() == Head::Waiting);
    CHECK(q.DropExpired(t0 + 121s) == 1);
    CHECK(q.Size() == 0);
}

TEST(Popup_WrongStateCallsDoNothing)
{
    PopupQueue q;
    q.Push(json::object(), t0);
    q.MarkShown(t0);
    CHECK(q.HeadState() == Head::Waiting);
    q.MarkRefused();
    CHECK(q.HeadState() == Head::Waiting);
    q.MarkRequested(t0);
    q.MarkRequested(t0 + 5s);
    CHECK(q.HeadState() == Head::Requested);
    CHECK(!q.ShownTimedOut(t0 + 10s));
}

TEST(Popup_DropExpired)
{
    PopupQueue q;
    q.Push(json::object(), t0);
    q.Push(json::object(), t0 + 60s);
    q.Push(json::object(), t0 + 119s);
    CHECK(q.DropExpired(t0 + 121s) == 1);
    CHECK(q.Size() == 2);
    CHECK(q.HeadState() == Head::Waiting);
}

TEST(Popup_DropExpiredSparesRequestedAndShown)
{
    PopupQueue q;
    q.Push(json::object(), t0);
    q.MarkRequested(t0);
    CHECK(q.DropExpired(t0 + 500s) == 0);
    q.MarkShown(t0 + 1s);
    CHECK(q.DropExpired(t0 + 500s) == 0);
    CHECK(q.Size() == 1);
}

TEST(Popup_DropExpiredBehindLiveHead)
{
    PopupQueue q;
    q.Push(json::object(), t0);
    q.Push(json::object(), t0 + 1s);
    q.MarkRequested(t0);
    CHECK(q.DropExpired(t0 + 500s) == 1);
    CHECK(q.Size() == 1);
    CHECK(q.HeadState() == Head::Requested);
}

TEST(Popup_DismissAndClear)
{
    PopupQueue q;
    q.Push(json{ { "n", 1 } }, t0);
    q.Push(json{ { "n", 2 } }, t0);
    q.MarkRequested(t0);
    q.Dismiss();
    CHECK(q.Size() == 1);
    CHECK(q.HeadState() == Head::Waiting);
    CHECK((*q.HeadPayload())["n"] == 2);
    q.Clear();
    CHECK(q.HeadState() == Head::None);
    CHECK(q.Size() == 0);
}

TEST(Popup_SequenceNumbers)
{
    PopupQueue q;
    CHECK(q.HeadSeq() == 0);
    CHECK(q.Push(json{ { "n", 1 } }, t0) == 1);
    CHECK(q.Push(json{ { "n", 2 } }, t0) == 2);
    CHECK(q.HeadSeq() == 1);
    CHECK((*q.HeadPayload())["seq"] == 1);
    q.Dismiss();
    CHECK(q.HeadSeq() == 2);
    CHECK((*q.HeadPayload())["seq"] == 2);
    q.Clear();
    CHECK(q.HeadSeq() == 0);
    // never reused in a session, also after Clear
    CHECK(q.Push(json::object(), t0) == 3);
    CHECK(q.HeadSeq() == 3);
    // a payload that is not an object still gets a number, kept beside it
    q.Clear();
    CHECK(q.Push(json::array(), t0) == 4);
    CHECK(q.HeadSeq() == 4);
    CHECK(q.HeadPayload()->is_array());
}

TEST(Popup_DismissDropsARefusedHead)
{
    // The host refused a head the page had already drawn; the page's Dismissed (matching seq) drops it.
    PopupQueue q;
    const auto seq = q.Push(json::object(), t0);
    q.Push(json::object(), t0);
    q.MarkRequested(t0);
    q.MarkRefused();
    CHECK(q.HeadState() == Head::Waiting);
    CHECK(q.HeadSeq() == seq);
    q.Dismiss();
    CHECK(q.Size() == 1);
    CHECK(q.HeadSeq() == seq + 1);
    CHECK(q.HeadState() == Head::Waiting);
}

TEST(Popup_ShownTimeoutDropsTheWholeQueue)
{
    // A page that never reports a popup shown is not answering: the head and everything behind it go at once, so
    // the controls are not held for another timeout per waiting popup.
    PopupQueue q;
    q.Push(json::object(), t0);
    q.Push(json::object(), t0 + 1s);
    q.Push(json::object(), t0 + 2s);
    q.MarkRequested(t0);
    CHECK(q.DropAllIfShownTimedOut(t0 + 10s) == 0);
    CHECK(q.Size() == 3);
    CHECK(q.DropAllIfShownTimedOut(t0 + 11s) == 3);
    CHECK(q.Size() == 0);
    CHECK(q.HeadState() == Head::None);
    CHECK(q.DropAllIfShownTimedOut(t0 + 100s) == 0);
}

TEST(Popup_ShownTimeoutSparesWaitingAndShown)
{
    PopupQueue q;
    q.Push(json::object(), t0);
    q.Push(json::object(), t0);
    CHECK(q.DropAllIfShownTimedOut(t0 + 100s) == 0);   // Waiting head: not sent, nothing timed out
    q.MarkRequested(t0 + 100s);
    q.MarkShown(t0 + 101s);
    CHECK(q.DropAllIfShownTimedOut(t0 + 200s) == 0);   // Shown head: the page answered
    CHECK(q.Size() == 2);
}
