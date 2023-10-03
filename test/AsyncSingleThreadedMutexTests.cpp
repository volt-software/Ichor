#include "Common.h"
#include <ichor/stl/AsyncSingleThreadedMutex.h>
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/events/RunFunctionEvent.h>


TEST_CASE("AsyncSingleThreadedMutexTests") {
    AsyncSingleThreadedMutex m;
    AsyncSingleThreadedLockGuard *lg{};

    SECTION("Lock/unlock") {
        auto queue = std::make_unique<MultimapQueue>();
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

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            started_async_func = true;
            AsyncSingleThreadedLockGuard lg3 = co_await m.lock();
            unlocked = true;
            co_return {};
        });

        dm.runForOrQueueEmpty();

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
