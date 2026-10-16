#include "Common.h"
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <vector>

static_assert(!std::is_copy_constructible_v<Ichor::AsyncManualResetEvent>);
static_assert(!std::is_move_constructible_v<Ichor::AsyncManualResetEvent>);

namespace {
    EagerCoroutine awaitAndRecord(Ichor::AsyncManualResetEvent &evt, std::vector<int> &order, int id) {
        co_await evt;
        order.push_back(id);
    }
}

TEST_CASE("AsyncManualResetEventTests") {

    SECTION("default constructor initially not set") {
        Ichor::AsyncManualResetEvent event;
        CHECK(!event.is_set());
    }

    SECTION("construct event initially set") {
        Ichor::AsyncManualResetEvent event{true};
        CHECK(event.is_set());
    }

    SECTION("set and reset") {
        Ichor::AsyncManualResetEvent event;
        CHECK(!event.is_set());
        event.set();
        CHECK(event.is_set());
        event.set();
        CHECK(event.is_set());
        event.reset();
        CHECK(!event.is_set());
        event.reset();
        CHECK(!event.is_set());
        event.set();
        CHECK(event.is_set());
    }

    SECTION("set resumes all waiters in FIFO order") {
        Ichor::AsyncManualResetEvent event;
        std::vector<int> order;
        awaitAndRecord(event, order, 1);
        awaitAndRecord(event, order, 2);
        awaitAndRecord(event, order, 3);
        REQUIRE(order.empty());

        event.set();
        REQUIRE(order == std::vector<int>{1, 2, 3});

        // awaiting a set event does not suspend
        awaitAndRecord(event, order, 4);
        REQUIRE(order == std::vector<int>{1, 2, 3, 4});
    }
}
