#pragma once

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace romantasy
{
    // A recursive lock that holds work meant for another queue until the calling
    // thread's outermost Scope has released the mutex. SKSE runs queued tasks while
    // holding its own queue lock, so queuing one while this lock is held lets a
    // Papyrus thread and the main thread each wait on the other.
    class OutboxLock
    {
    public:
        using Task = std::function<void()>;
        using Dispatcher = std::function<void(Task)>;

        explicit OutboxLock(Dispatcher dispatcher) :
            _dispatcher(std::move(dispatcher))
        {}

        OutboxLock(const OutboxLock&) = delete;
        OutboxLock& operator=(const OutboxLock&) = delete;

        class Scope
        {
        public:
            explicit Scope(const OutboxLock& lock) :
                _lock(lock)
            {
                _lock._mutex.lock();
                ++_lock._depth;
            }

            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;
            Scope(Scope&&) = delete;
            Scope& operator=(Scope&&) = delete;

            // Outermost scope: run the hook and take the outbox while still locked, then
            // unlock, then hand each task to the dispatcher in the order it was deferred.
            ~Scope() noexcept(false)
            {
                std::unique_lock held(_lock._mutex, std::adopt_lock);
                if (_lock._depth > 1) {
                    --_lock._depth;
                    return;
                }

                std::vector<Task> outbox;
                try {
                    // Depth is still 1 here, so a Scope opened by the hook only nests.
                    if (_lock._beforeRelease) {
                        _lock._beforeRelease();
                    }
                } catch (...) {
                    _lock._depth = 0;
                    outbox.swap(_lock._outbox);
                    throw;
                }
                outbox.swap(_lock._outbox);
                _lock._depth = 0;
                held.unlock();

                for (auto& task : outbox) {
                    _lock._dispatcher(std::move(task));
                }
            }

        private:
            const OutboxLock& _lock;
        };

        // Call only inside a Scope opened on the calling thread.
        void Defer(Task task) const
        {
            _outbox.push_back(std::move(task));
        }

        // Runs under the mutex just before the outermost Scope unlocks.
        // Call before the lock is shared between threads.
        // If the hook throws, the tasks deferred in that scope are dropped.
        void SetBeforeRelease(std::function<void()> hook)
        {
            _beforeRelease = std::move(hook);
        }

    private:
        Dispatcher _dispatcher;
        std::function<void()> _beforeRelease;
        mutable std::recursive_mutex _mutex;
        // Touched only while _mutex is held.
        mutable int _depth = 0;
        mutable std::vector<Task> _outbox;
    };
}
