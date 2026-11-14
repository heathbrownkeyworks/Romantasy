#include "check.h"
#include "romance/OutboxLock.h"

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>

using romantasy::OutboxLock;

TEST(OutboxLock_DispatchesAfterOutermostScope)
{
    std::vector<std::string> dispatched;
    OutboxLock lock([&](OutboxLock::Task task) { task(); });
    {
        OutboxLock::Scope outer(lock);
        {
            OutboxLock::Scope inner(lock);
            lock.Defer([&] { dispatched.push_back("A"); });
        }
        CHECK(dispatched.empty());
        lock.Defer([&] { dispatched.push_back("B"); });
        CHECK(dispatched.empty());
    }
    CHECK(dispatched.size() == 2);
    CHECK(dispatched.size() == 2 && dispatched[0] == "A" && dispatched[1] == "B");
}

TEST(OutboxLock_MutexIsFreeAtDispatch)
{
    // Declared before the future so the lock outlives the async task that uses it.
    std::unique_ptr<OutboxLock> lock;
    std::future<void> other;
    bool otherReady = false;
    lock = std::make_unique<OutboxLock>([&](OutboxLock::Task) {
        other = std::async(std::launch::async, [&] { OutboxLock::Scope scope(*lock); });
        otherReady = other.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    });
    {
        OutboxLock::Scope scope(*lock);
        lock->Defer([] {});
    }
    CHECK(otherReady);
}

TEST(OutboxLock_DeferDuringDispatchIsDelivered)
{
    std::vector<std::string> dispatched;
    std::unique_ptr<OutboxLock> lock;
    lock = std::make_unique<OutboxLock>([&](OutboxLock::Task task) { task(); });
    {
        OutboxLock::Scope scope(*lock);
        lock->Defer([&] {
            dispatched.push_back("first");
            OutboxLock::Scope inner(*lock);
            lock->Defer([&] { dispatched.push_back("C"); });
        });
    }
    CHECK(dispatched.size() == 2);
    CHECK(dispatched.size() == 2 && dispatched[0] == "first" && dispatched[1] == "C");
}

TEST(OutboxLock_HookRunsOncePerOutermostScope)
{
    // Declared before the future so the lock outlives the async task that uses it.
    std::unique_ptr<OutboxLock> lock;
    std::future<void> other;
    int hookCount = 0;
    int dispatchedBeforeHook = -1;
    int dispatchCount = 0;
    bool otherBlockedDuringHook = false;
    lock = std::make_unique<OutboxLock>([&](OutboxLock::Task task) {
        ++dispatchCount;
        task();
    });
    const auto testThread = std::this_thread::get_id();
    lock->SetBeforeRelease([&] {
        // The probe thread's own Scope also runs the hook; count only this thread's.
        if (std::this_thread::get_id() != testThread) {
            return;
        }
        ++hookCount;
        dispatchedBeforeHook = dispatchCount;
        // The hook runs under the mutex, so another thread cannot open a Scope yet.
        other = std::async(std::launch::async, [&] { OutboxLock::Scope scope(*lock); });
        otherBlockedDuringHook = other.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout;
    });
    {
        OutboxLock::Scope outer(*lock);
        {
            OutboxLock::Scope inner(*lock);
            lock->Defer([] {});
        }
        CHECK(hookCount == 0);
        lock->Defer([] {});
    }
    other.get();
    CHECK(hookCount == 1);
    CHECK(dispatchedBeforeHook == 0);
    CHECK(dispatchCount == 2);
    CHECK(otherBlockedDuringHook);
}

TEST(OutboxLock_EmptyScopeDispatchesNothing)
{
    int dispatchCount = 0;
    OutboxLock lock([&](OutboxLock::Task) { ++dispatchCount; });
    {
        OutboxLock::Scope outer(lock);
        OutboxLock::Scope inner(lock);
    }
    CHECK(dispatchCount == 0);
}
