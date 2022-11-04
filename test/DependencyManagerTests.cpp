#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include "TestServices/UselessService.h"
#include "Common.h"

TEST_CASE("DependencyManager") {
    SECTION("ExceptionOnStart_WhenNoRegistrations") {
        std::atomic<bool> stopped = false;
        std::atomic<bool> thrown_exception = false;
        std::thread t([&]() {
            try {
                auto queue = std::make_unique<MultimapQueue>();
                auto &dm = queue->createManager();
                queue->start(CaptureSigInt);
            } catch (...) {
                thrown_exception = true;
            }
            stopped = true;
        });

        t.join();

        REQUIRE(stopped);
        REQUIRE(thrown_exception);
    }

    SECTION("DependencyManager", "ExceptionOnStart_WhenNoFrameworkLogger") {
        std::atomic<bool> stopped = false;
        std::atomic<bool> thrown_exception = false;
        std::thread t([&]() {
            try {
                auto queue = std::make_unique<MultimapQueue>();
                auto &dm = queue->createManager();
                dm.createServiceManager<UselessService>();
                queue->start(CaptureSigInt);
            } catch (...) {
                thrown_exception = true;
            }
            stopped = true;
        });

        t.join();

        REQUIRE(stopped);
        REQUIRE(thrown_exception);
    }

    SECTION("DependencyManager", "QuitOnQuitEvent") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService>();
            queue->start(CaptureSigInt);
        });

        dm.runForOrQueueEmpty();

        REQUIRE(dm.isRunning());

        dm.pushEvent<QuitEvent>(0);

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("DependencyManager", "RunFunctionEvent thread") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        std::thread::id testThreadId = std::this_thread::get_id();
        std::thread::id dmThreadId;

        REQUIRE(Ichor::Detail::_local_dm == nullptr);

        std::thread t([&]() {
            dmThreadId = std::this_thread::get_id();
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService>();
            queue->start(CaptureSigInt);
        });

        dm.runForOrQueueEmpty();

        REQUIRE(dm.isRunning());

        REQUIRE(Ichor::Detail::_local_dm == nullptr);

        AsyncManualResetEvent evt;

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager &_dm) -> AsyncGenerator<void> {
            REQUIRE(Ichor::Detail::_local_dm == &_dm);
            REQUIRE(Ichor::Detail::_local_dm == &dm);
            REQUIRE(testThreadId != std::this_thread::get_id());
            REQUIRE(dmThreadId == std::this_thread::get_id());
            co_await evt;
            REQUIRE(Ichor::Detail::_local_dm == &_dm);
            REQUIRE(Ichor::Detail::_local_dm == &dm);
            REQUIRE(testThreadId != std::this_thread::get_id());
            REQUIRE(dmThreadId == std::this_thread::get_id());
            dm.pushEvent<QuitEvent>(0);
            co_return;
        });

        dm.runForOrQueueEmpty();

        REQUIRE(Ichor::Detail::_local_dm == nullptr);

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager &_dm) -> AsyncGenerator<void> {
            REQUIRE(Ichor::Detail::_local_dm == &_dm);
            REQUIRE(Ichor::Detail::_local_dm == &dm);
            REQUIRE(testThreadId != std::this_thread::get_id());
            REQUIRE(dmThreadId == std::this_thread::get_id());
            evt.set();
            co_return;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }
}