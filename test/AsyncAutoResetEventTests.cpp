#include "Common.h"
#include <ichor/coroutines/AsyncAutoResetEvent.h>
#include <vector>

static_assert(!std::is_copy_constructible_v<Ichor::AsyncAutoResetEvent>);
static_assert(!std::is_move_constructible_v<Ichor::AsyncAutoResetEvent>);

namespace {
    EagerCoroutine awaitAndRecord(Ichor::AsyncAutoResetEvent &evt, std::vector<int> &order, int id) {
        co_await evt;
        order.push_back(id);
    }

    EagerCoroutine awaitTwiceAndRecord(Ichor::AsyncAutoResetEvent &evt, std::vector<int> &order, int id) {
        co_await evt;
        order.push_back(id);
        co_await evt;
        order.push_back(id);
    }
}

TEST_CASE("AsyncAutoResetEventTests") {
    std::vector<int> order;

    SECTION("awaiting a set event does not suspend and resets it") {
        Ichor::AsyncAutoResetEvent evt{true};
        awaitAndRecord(evt, order, 1);
        REQUIRE(order == std::vector<int>{1});

        awaitAndRecord(evt, order, 2);
        REQUIRE(order == std::vector<int>{1});
        evt.set();
        REQUIRE(order == std::vector<int>{1, 2});
    }

    SECTION("set resumes one waiter at a time in FIFO order") {
        Ichor::AsyncAutoResetEvent evt;
        awaitAndRecord(evt, order, 1);
        awaitAndRecord(evt, order, 2);
        awaitAndRecord(evt, order, 3);
        REQUIRE(order.empty());

        evt.set();
        REQUIRE(order == std::vector<int>{1});
        evt.set();
        REQUIRE(order == std::vector<int>{1, 2});
        evt.set();
        REQUIRE(order == std::vector<int>{1, 2, 3});

        // No waiters left, so the event stays set until the next await.
        evt.set();
        awaitAndRecord(evt, order, 4);
        REQUIRE(order == std::vector<int>{1, 2, 3, 4});
    }

    SECTION("set is a no-op if already set") {
        Ichor::AsyncAutoResetEvent evt;
        evt.set();
        evt.set();
        awaitAndRecord(evt, order, 1);
        awaitAndRecord(evt, order, 2);
        REQUIRE(order == std::vector<int>{1});
        evt.set();
        REQUIRE(order == std::vector<int>{1, 2});
    }

    SECTION("reset") {
        Ichor::AsyncAutoResetEvent evt{true};
        evt.reset();
        awaitAndRecord(evt, order, 1);
        REQUIRE(order.empty());
        evt.set();
        REQUIRE(order == std::vector<int>{1});
    }

    SECTION("set_all resumes all waiters, including ones that await again, and leaves the event set") {
        Ichor::AsyncAutoResetEvent evt;
        awaitTwiceAndRecord(evt, order, 1);
        awaitAndRecord(evt, order, 2);

        evt.set_all();
        REQUIRE(order == std::vector<int>{1, 2, 1});

        awaitAndRecord(evt, order, 3);
        REQUIRE(order == std::vector<int>{1, 2, 1, 3});
    }
}
