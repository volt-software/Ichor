#include "Common.h"
#include <ichor/coroutines/AsyncGenerator.h>
#include <ichor/coroutines/Task.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <memory>
#include <string>

// Lvalues are copied when yielded/returned, so move-only types have to be moved explicitly.
template <typename Promise, typename V>
concept CanYield = requires(Promise &p, V &&v) { p.yield_value(std::forward<V>(v)); };
template <typename Promise, typename V>
concept CanReturn = requires(Promise &p, V &&v) { p.return_value(std::forward<V>(v)); };
using MoveOnlyPromise = Ichor::Detail::AsyncGeneratorPromise<std::unique_ptr<int>>;
static_assert(!CanYield<MoveOnlyPromise, std::unique_ptr<int>&>);
static_assert(CanYield<MoveOnlyPromise, std::unique_ptr<int>&&>);
static_assert(!CanReturn<MoveOnlyPromise, std::unique_ptr<int>&>);
static_assert(CanReturn<MoveOnlyPromise, std::unique_ptr<int>&&>);
using CopyablePromise = Ichor::Detail::AsyncGeneratorPromise<std::string>;
static_assert(CanYield<CopyablePromise, std::string const &>);
static_assert(CanReturn<CopyablePromise, std::string const &>);

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

TEST_CASE("TaskTests: moving a suspended generator transfers ownership") {
    _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
    started = false;
    ran = false;
    auto gen = start_task(task_waiting_on_evt);
    auto it = gen.begin();
    REQUIRE(!it.get_finished());

    Ichor::AsyncGenerator<void> moved{std::move(gen)};
    CHECK(gen.done());
    CHECK(!moved.done());

    _evt->set();
    REQUIRE(ran == true);
    CHECK(moved.done());

    // Move assignment releases the previous coroutine and takes over the new one.
    moved = start_task(task_immediate_return);
    CHECK(!moved.done());
    auto it2 = moved.begin();
    CHECK(it2.get_finished());
    CHECK(moved.done());
}

TEST_CASE("TaskTests: destroying a generator while its coroutine is suspended elsewhere detaches it") {
    _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
    started = false;
    ran = false;
    {
        auto gen = start_task(task_waiting_on_evt);
        auto it = gen.begin();
        REQUIRE(!it.get_finished());
        REQUIRE(started == true);
    }

    // The coroutine is cancelled and destroys itself once it reaches its final suspend point,
    // without touching the destroyed AsyncGenerator.
    _evt->set();
    REQUIRE(ran == true);
}
