#pragma once

#include <chrono>
#include <optional>

namespace romantasy
{
    // Answers "has the interval passed since the last time this said yes?". The time is
    // passed in so callers decide the clock and tests need no sleeping.
    class Throttle
    {
    public:
        explicit Throttle(std::chrono::steady_clock::duration interval) :
            _interval(interval)
        {}

        // True on first use, after Reset, or once the interval has passed since the last
        // true; a true re-arms the interval from `now`.
        [[nodiscard]] bool Due(std::chrono::steady_clock::time_point now)
        {
            if (_last && now - *_last < _interval) {
                return false;
            }
            _last = now;
            return true;
        }

        void Reset() { _last.reset(); }

    private:
        std::chrono::steady_clock::duration _interval;
        std::optional<std::chrono::steady_clock::time_point> _last;
    };
}
