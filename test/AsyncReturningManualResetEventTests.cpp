#include "Common.h"
#include <ichor/coroutines/AsyncReturningManualResetEvent.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/event_queues/PriorityQueue.h>
#include "TestServices/AsyncReturningManualResetEventService.h"

AsyncReturningManualResetEvent<int> _evtInt;
AsyncReturningManualResetEvent<NoCopyNoMove> _evtNoCopyNoMove;

uint64_t NoCopyNoMove::countConstructed{};
uint64_t NoCopyNoMove::countDestructed{};

TEST_CASE("AsyncReturningManualResetEvent") {
    _evtInt.reset();
    _evtNoCopyNoMove.reset();

    SECTION("default constructor initially not set") {
        AsyncReturningManualResetEvent<int> event;
        CHECK(!event.is_set());
    }

    SECTION("construct event initially set") {
        AsyncReturningManualResetEvent<int> event{true};
        CHECK(event.is_set());
    }

    SECTION("set and reset") {
        AsyncReturningManualResetEvent<int> event;
        CHECK(!event.is_set());
        event.set(1);
        CHECK(event.is_set());
        event.set(2);
        CHECK(event.is_set());
        event.reset();
        CHECK(!event.is_set());
        event.reset();
        CHECK(!event.is_set());
        event.set(3);
        CHECK(event.is_set());
    }

    SECTION("construct event initially set") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        ServiceIdType id{};
        std::thread t([&] {
            id = dm.createServiceManager<AwaitReturningManualResetService, IAwaitReturningManualResetService>()->getServiceId();
            queue->start(DoNotCaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);
        NoCopyNoMove nocopy;

        queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto svc = dm.getService<IAwaitReturningManualResetService>(id);
            REQUIRE(svc);

            queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
                _evtInt.set(5);
            });

            auto intRet = co_await _evtInt;
            REQUIRE(intRet == 5);

            queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
                _evtNoCopyNoMove.set(nocopy);
            });

            auto const &awaitedNocopy = co_await _evtNoCopyNoMove;
            REQUIRE(awaitedNocopy.countConstructed == 1);
            REQUIRE(awaitedNocopy.countDestructed == 0);

            queue->pushEvent<QuitEvent>(ServiceIdType{0});

            co_return {};
        });

        t.join();
    }
}
