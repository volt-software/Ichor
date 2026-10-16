#include "Common.h"
#include <ichor/coroutines/AsyncGenerator.h>
#include <ichor/coroutines/Task.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <memory>

namespace {
    std::unique_ptr<Ichor::AsyncManualResetEvent> _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
    bool started{};
    bool ran{};

    Ichor::Task<void> task_immediate_return() {
        started = true;
        ran = true;
        co_return;
    }

    Ichor::Task<void> task_waiting_on_evt() {
        started = true;
        co_await *_evt;
        ran = true;
        co_return;
    }

    Ichor::Task<void> task_waiting_on_immediate_return_task() {
        co_await task_immediate_return();
        co_return;
    }

    Ichor::Task<void> task_waiting_on_evt_task() {
        co_await task_waiting_on_evt();
        co_return;
    }

    Ichor::Task<uint64_t> task_return_value(uint64_t i) {
        co_return i;
    }

    uint64_t deepSum{};

    // Awaits many synchronously completing tasks in a row. Without symmetric transfer every iteration adds stack frames.
    Ichor::Task<void> task_awaiting_many_immediate_return_tasks() {
        uint64_t sum{};
        for(uint64_t i = 0; i < 1'000'000; ++i) {
            sum += co_await task_return_value(i);
        }
        deepSum = sum;
        co_return;
    }

    template<typename LAMBDA>
    Ichor::AsyncGenerator<void> start_task(LAMBDA *l) {
        co_await l();
        co_return;
    }
}

TEST_CASE("TaskTests: default constructor initially not ready") {
    Ichor::Task t;
    CHECK(!t.is_ready());
}

TEST_CASE("TaskTests: resumes on evt") {
    auto gen = start_task(task_waiting_on_evt);
    auto it = gen.begin();
    REQUIRE(!it.get_finished());
    REQUIRE(!gen.done());
    REQUIRE(started == true);
    REQUIRE(ran == false);
    _evt->set();
    REQUIRE(ran == true);
    REQUIRE(it.get_finished());
    REQUIRE(gen.done());
}

TEST_CASE("TaskTests: done on immediate return") {
    auto gen = start_task(task_immediate_return);
    auto it = gen.begin();
    CHECK(it.get_finished());
    CHECK(gen.done());
    CHECK(started == true);
    CHECK(ran == true);
}

TEST_CASE("TaskTests: done on nested resume on evt") {
    auto gen = start_task(task_waiting_on_evt_task);
    auto it = gen.begin();
    REQUIRE(!it.get_finished());
    REQUIRE(!gen.done());
    REQUIRE(started == true);
    REQUIRE(ran == false);
    _evt->set();
    REQUIRE(ran == true);
    REQUIRE(it.get_finished());
    REQUIRE(gen.done());
}

TEST_CASE("TaskTests: done on nested immediate return") {
    auto gen = start_task(task_waiting_on_immediate_return_task);
    auto it = gen.begin();
    CHECK(it.get_finished());
    CHECK(gen.done());
    CHECK(started == true);
    CHECK(ran == true);
}

TEST_CASE("TaskTests: awaiting many immediate return tasks does not grow the stack") {
    deepSum = 0;
    auto gen = start_task(task_awaiting_many_immediate_return_tasks);
    auto it = gen.begin();
    CHECK(it.get_finished());
    CHECK(gen.done());
    CHECK(deepSum == 499'999'500'000ull);
}
