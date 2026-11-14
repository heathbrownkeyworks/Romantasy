#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>

#include <nlohmann/json.hpp>

namespace romantasy::ui
{
    // Level-change popups wait here until the page can show them. Time is
    // always passed in; the queue never reads the clock.
    class PopupQueue
    {
    public:
        using Clock = std::chrono::steady_clock;
        static constexpr std::chrono::seconds kRetryInterval{ 1 }, kMaxWait{ 120 }, kShownTimeout{ 10 }, kMaxShown{ 30 };

        // None; Waiting = not sent to the page yet; Requested = sent, not yet
        // reported on screen; Shown = on screen. Entries behind the head are Waiting.
        enum class Head { None, Waiting, Requested, Shown };

        // Queues a popup under the next sequence number (from 1, never reused while this queue lives) and returns it.
        // An object payload carries the number as the integer "seq", which the page echoes in Shown and Dismissed.
        std::uint32_t Push(nlohmann::json payload, Clock::time_point now);
        [[nodiscard]] Head HeadState() const;
        [[nodiscard]] const nlohmann::json* HeadPayload() const;
        [[nodiscard]] std::uint32_t HeadSeq() const;  // 0 when the queue is empty
        [[nodiscard]] std::size_t Size() const;

        // True when a Waiting head should be tried now: `force` (a menu just
        // closed, or the ledger is open), or kRetryInterval since the last try. Records the try.
        [[nodiscard]] bool ShouldAttempt(Clock::time_point now, bool force);
        void MarkRequested(Clock::time_point now);  // Waiting -> Requested
        void MarkRefused();                         // Requested -> Waiting; the wait clock keeps running
        void MarkShown(Clock::time_point now);      // Requested -> Shown
        void Dismiss();                             // drops the head, whatever its state
        void Clear();

        // Drops Waiting entries (head and behind it) queued longer than kMaxWait; returns how many.
        std::size_t DropExpired(Clock::time_point now);
        [[nodiscard]] bool ShownTimedOut(Clock::time_point now) const;  // Requested for longer than kShownTimeout
        // When ShownTimedOut: the page is not answering, so the whole queue goes; returns how many were dropped.
        std::size_t DropAllIfShownTimedOut(Clock::time_point now);
        [[nodiscard]] bool ShownTooLong(Clock::time_point now) const;   // Shown for longer than kMaxShown

    private:
        struct Entry
        {
            nlohmann::json payload;
            std::uint32_t seq = 0;
            Clock::time_point queuedAt;
            Head state = Head::Waiting;
            Clock::time_point stateSince;  // when it became Requested or Shown
            bool tried = false;
            Clock::time_point lastTry;
        };

        std::deque<Entry> _entries;
        std::uint32_t _nextSeq = 1;
    };
}
