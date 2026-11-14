#include "check.h"
#include "romance/Throttle.h"

#include <chrono>

using romantasy::Throttle;
using namespace std::chrono_literals;

namespace
{
    const auto kStart = std::chrono::steady_clock::time_point{} + 1000s;
}

TEST(Throttle_FirstCallIsDue)
{
    Throttle throttle(5s);
    CHECK(throttle.Due(kStart));
}

TEST(Throttle_NotDueInsideInterval)
{
    Throttle throttle(5s);
    CHECK(throttle.Due(kStart));
    CHECK(!throttle.Due(kStart));
    CHECK(!throttle.Due(kStart + 2s));
    CHECK(!throttle.Due(kStart + 4999ms));
}

TEST(Throttle_DueAfterInterval)
{
    Throttle throttle(5s);
    CHECK(throttle.Due(kStart));
    CHECK(throttle.Due(kStart + 5s));
    // Re-armed at the second call, not at the first.
    CHECK(!throttle.Due(kStart + 9s));
    CHECK(throttle.Due(kStart + 11s));
}

TEST(Throttle_EarlierClockIsNotDue)
{
    Throttle throttle(5s);
    CHECK(throttle.Due(kStart));
    CHECK(!throttle.Due(kStart - 10s));
    // The earlier time did not re-arm: the interval still runs from kStart.
    CHECK(!throttle.Due(kStart + 4s));
    CHECK(throttle.Due(kStart + 5s));
}

TEST(Throttle_ResetMakesNextCallDue)
{
    Throttle throttle(5s);
    CHECK(throttle.Due(kStart));
    throttle.Reset();
    CHECK(throttle.Due(kStart + 1s));
    CHECK(!throttle.Due(kStart + 2s));
}
