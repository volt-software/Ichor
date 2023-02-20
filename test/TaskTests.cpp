#include "Common.h"
#include <ichor/coroutines/AsyncGenerator.h>
#include <ichor/coroutines/Task.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <memory>

std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;
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

template<typename LAMBDA>
Ichor::AsyncGenerator<void> start_task(LAMBDA *l) {
    co_await l();
    co_return;
}

TEST_CASE("TaskTests") {
    _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
    ran = false;
    started = false;

    SECTION("default constructor initially not ready") {
        Ichor::Task t;
        CHECK(!t.is_ready());
    }

    SECTION("resumes on evt") {
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

    SECTION("done on immediate return") {
        auto gen = start_task(task_immediate_return);
        auto it = gen.begin();
        CHECK(it.get_finished());
        CHECK(gen.done());
        CHECK(started == true);
        CHECK(ran == true);
    }

    SECTION("done on nested resume on evt") {
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

    SECTION("done on nested immediate return") {
        auto gen = start_task(task_waiting_on_immediate_return_task);
        auto it = gen.begin();
        CHECK(it.get_finished());
        CHECK(gen.done());
        CHECK(started == true);
        CHECK(ran == true);
    }

    _evt.reset();
}
