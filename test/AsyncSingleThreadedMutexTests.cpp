#include "Common.h"
#include <ichor/stl/AsyncSingleThreadedMutex.h>
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/events/RunFunctionEvent.h>

TEST_CASE("AsyncSingleThreadedMutexTests: Lock/unlock") {
    AsyncSingleThreadedMutex m;
    AsyncSingleThreadedLockGuard *lg{};
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
