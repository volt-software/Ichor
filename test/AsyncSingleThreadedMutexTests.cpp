#include "Common.h"
#include <ichor/stl/AsyncSingleThreadedMutex.h>
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/events/RunFunctionEvent.h>


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

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto val = m.non_blocking_lock();
            REQUIRE(!val);
        });

#ifdef ICHOR_MUSL
        std::this_thread::sleep_for(50ms);
        dm.runForOrQueueEmpty(1'000ms);
#else
        dm.runForOrQueueEmpty();
#endif

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
//            fmt::print("RunFunctionEventAsync\n");
            started_async_func = true;
            AsyncSingleThreadedLockGuard lg3 = co_await m.lock();
            unlocked = true;
            co_return {};
        });

#ifdef ICHOR_MUSL
        std::this_thread::sleep_for(50ms);
        dm.runForOrQueueEmpty(1'000ms);
#else
        dm.runForOrQueueEmpty();
#endif
//        fmt::print("REQUIRE(started_async_func)\n");
        REQUIRE(started_async_func);
        REQUIRE(!unlocked);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            lg->unlock();
        });

        dm.runForOrQueueEmpty();

        REQUIRE(unlocked);

        queue->pushEvent<QuitEvent>(0);

        t.join();
    }
}
