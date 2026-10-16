#include "Common.h"
#include <ichor/coroutines/AsyncReturningManualResetEvent.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/event_queues/PriorityQueue.h>
#include "TestServices/AsyncReturningManualResetEventService.h"
#include <vector>

static_assert(!std::is_copy_constructible_v<AsyncReturningManualResetEvent<int>>);
static_assert(!std::is_move_constructible_v<AsyncReturningManualResetEvent<int>>);

namespace {
    EagerCoroutine awaitAndRecord(AsyncReturningManualResetEvent<int> &evt, std::vector<std::pair<int, int>> &order, int id) {
        int const &value = co_await evt;
        order.emplace_back(id, value);
    }
}

AsyncReturningManualResetEvent<int> _evtInt;
AsyncReturningManualResetEvent<NoCopyNoMove> _evtNoCopyNoMove;
uint64_t NoCopyNoMove::countConstructed{};
uint64_t NoCopyNoMove::countDestructed{};

TEST_CASE("AsyncReturningManualResetEvent: default constructor initially not set") {
    AsyncReturningManualResetEvent<int> event;
    CHECK(!event.is_set());
}

TEST_CASE("AsyncReturningManualResetEvent: construct event initially set") {
    AsyncReturningManualResetEvent<int> event{true};
    CHECK(event.is_set());
}

TEST_CASE("AsyncReturningManualResetEvent: set resumes all waiters in FIFO order with the value") {
    AsyncReturningManualResetEvent<int> event;
    std::vector<std::pair<int, int>> order;
    awaitAndRecord(event, order, 1);
    awaitAndRecord(event, order, 2);
    REQUIRE(order.empty());

    int const value = 42;
    event.set(value);
    REQUIRE(order == std::vector<std::pair<int, int>>{{1, 42}, {2, 42}});

    awaitAndRecord(event, order, 3);
    REQUIRE(order == std::vector<std::pair<int, int>>{{1, 42}, {2, 42}, {3, 42}});
}

TEST_CASE("AsyncReturningManualResetEvent: set and reset") {
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

TEST_CASE("AsyncReturningManualResetEvent: awaiting a no copy/no move class should work and return a reference.") {
    _evtInt.reset();
    _evtNoCopyNoMove.reset();
    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->getManager();
    ServiceIdType id{};
    std::thread t([&] {
        Ichor::Detail::_local_dm = &dm;
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
