#include "ui/PopupQueue.h"

#include <utility>

namespace romantasy::ui
{
    std::uint32_t PopupQueue::Push(nlohmann::json payload, Clock::time_point now)
    {
        Entry entry;
        entry.seq = _nextSeq++;
        if (payload.is_object()) {
            payload["seq"] = entry.seq;
        }
        entry.payload = std::move(payload);
        entry.queuedAt = now;
        _entries.push_back(std::move(entry));
        return _entries.back().seq;
    }

    PopupQueue::Head PopupQueue::HeadState() const
    {
        return _entries.empty() ? Head::None : _entries.front().state;
    }

    const nlohmann::json* PopupQueue::HeadPayload() const
    {
        return _entries.empty() ? nullptr : &_entries.front().payload;
    }

    std::uint32_t PopupQueue::HeadSeq() const
    {
        return _entries.empty() ? 0 : _entries.front().seq;
    }

    std::size_t PopupQueue::Size() const
    {
        return _entries.size();
    }

    bool PopupQueue::ShouldAttempt(Clock::time_point now, bool force)
    {
        if (HeadState() != Head::Waiting) {
            return false;
        }
        Entry& head = _entries.front();
        if (!force && head.tried && now - head.lastTry < kRetryInterval) {
            return false;
        }
        head.tried = true;
        head.lastTry = now;
        return true;
    }

    void PopupQueue::MarkRequested(Clock::time_point now)
    {
        if (HeadState() != Head::Waiting) {
            return;
        }
        _entries.front().state = Head::Requested;
        _entries.front().stateSince = now;
    }

    void PopupQueue::MarkRefused()
    {
        if (HeadState() == Head::Requested) {
            _entries.front().state = Head::Waiting;
        }
    }

    void PopupQueue::MarkShown(Clock::time_point now)
    {
        if (HeadState() != Head::Requested) {
            return;
        }
        _entries.front().state = Head::Shown;
        _entries.front().stateSince = now;
    }

    void PopupQueue::Dismiss()
    {
        if (!_entries.empty()) {
            _entries.pop_front();
        }
    }

    void PopupQueue::Clear()
    {
        _entries.clear();
    }

    std::size_t PopupQueue::DropExpired(Clock::time_point now)
    {
        std::size_t dropped = 0;
        for (auto it = _entries.begin(); it != _entries.end();) {
            if (it->state == Head::Waiting && now - it->queuedAt > kMaxWait) {
                it = _entries.erase(it);
                ++dropped;
            } else {
                ++it;
            }
        }
        return dropped;
    }

    bool PopupQueue::ShownTimedOut(Clock::time_point now) const
    {
        return HeadState() == Head::Requested && now - _entries.front().stateSince > kShownTimeout;
    }

    std::size_t PopupQueue::DropAllIfShownTimedOut(Clock::time_point now)
    {
        if (!ShownTimedOut(now)) {
            return 0;
        }
        const auto dropped = _entries.size();
        _entries.clear();
        return dropped;
    }

    bool PopupQueue::ShownTooLong(Clock::time_point now) const
    {
        return HeadState() == Head::Shown && now - _entries.front().stateSince > kMaxShown;
    }
}
