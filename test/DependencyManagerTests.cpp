#include <ichor/event_queues/MultimapQueue.h>
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
}