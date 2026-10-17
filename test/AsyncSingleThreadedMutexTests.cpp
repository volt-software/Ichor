#include "Common.h"
#include <ichor/stl/AsyncSingleThreadedMutex.h>
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <algorithm>
#include <vector>


TEST_CASE("AsyncSingleThreadedMutexTests") {
    AsyncSingleThreadedMutex m;
    AsyncSingleThreadedLockGuard *lg{};

    SECTION("Lock/unlock") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        bool started_async_func{};
        bool unlocked{};

        std::thread t([&]() {
            AsyncSingleThreadedLockGuard lg2 = m.non_blocking_lock().value();
            lg = &lg2;
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto val = m.non_blocking_lock();
            REQUIRE(!val);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
            started_async_func = true;
            AsyncSingleThreadedLockGuard lg3 = co_await m.lock();
            unlocked = true;
            co_return {};
        });

        runForOrQueueEmpty(dm);
        REQUIRE(started_async_func);
        REQUIRE(!unlocked);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            lg->unlock();
        });

        runForOrQueueEmpty(dm);

        REQUIRE(unlocked);

        queue->pushEvent<QuitEvent>(ServiceIdType{0});

        t.join();
    }
    SECTION("Multiple waiters get the lock one at a time") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        tl::optional<AsyncSingleThreadedLockGuard> first;
        AsyncManualResetEvent releaseWaiter1;
        AsyncManualResetEvent releaseWaiter2;
        uint32_t holders{};
        uint32_t maxHolders{};
        std::vector<uint32_t> acquiredOrder;

        std::thread t([&]() {
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            first = m.non_blocking_lock();
            REQUIRE(first);
        });

        runForOrQueueEmpty(dm);

        auto waiter = [&](uint32_t id, AsyncManualResetEvent &release) {
            return [&, id]() -> AsyncGenerator<IchorBehaviour> {
                AsyncSingleThreadedLockGuard lg = co_await m.lock();
                ++holders;
                maxHolders = std::max(maxHolders, holders);
                acquiredOrder.push_back(id);
                // keep holding the lock while suspended, so that a second holder would be noticed
                co_await release;
                --holders;
                co_return {};
            };
        };

        queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, waiter(1, releaseWaiter1));
        queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, waiter(2, releaseWaiter2));

        runForOrQueueEmpty(dm);
        REQUIRE(acquiredOrder.empty());

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            first->unlock();
        });

        runForOrQueueEmpty(dm);

        // Both waiters used to wait on the first waiter's event, so they acquired the lock at the same time.
        REQUIRE(acquiredOrder == std::vector<uint32_t>{1});
        REQUIRE(maxHolders == 1);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            releaseWaiter1.set();
        });

        runForOrQueueEmpty(dm);

        REQUIRE(acquiredOrder == std::vector<uint32_t>{1, 2});
        REQUIRE(maxHolders == 1);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            releaseWaiter2.set();
            REQUIRE(m.non_blocking_lock());
        });

        runForOrQueueEmpty(dm);

        REQUIRE(holders == 0);

        queue->pushEvent<QuitEvent>(ServiceIdType{0});

        t.join();
    }
}
