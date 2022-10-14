#include <ichor/event_queues/MultimapQueue.h>
#include "TestServices/UselessService.h"
#include "Common.h"

#if __has_include(<spdlog/spdlog.h>)
#include <ichor/optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#else
#include <ichor/optional_bundles/logging_bundle/NullFrameworkLogger.h>
#endif

TEST_CASE("DependencyManager") {

#if __has_include(<spdlog/spdlog.h>)
    //default logger is disabled in cmake
    if(spdlog::default_logger_raw() == nullptr) {
        auto new_logger = spdlog::stdout_color_st("new_default_logger");
        spdlog::set_default_logger(new_logger);
    }
#endif

    SECTION("ExceptionOnStart_WhenNoRegistrations") {
        std::atomic<bool> stopped = false;
        std::atomic<bool> thrown_exception = false;
        std::thread t([&]() {
            try {
                Ichor::DependencyManager dm{std::make_unique<MultimapQueue>()};
                dm.start();
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
                Ichor::DependencyManager dm{std::make_unique<MultimapQueue>()};
                dm.createServiceManager<UselessService>();
                dm.start();
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
        Ichor::DependencyManager dm{std::make_unique<MultimapQueue>()};

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService>();
            dm.start();
        });

        dm.runForOrQueueEmpty();

        REQUIRE(dm.isRunning());

        dm.pushEvent<QuitEvent>(0);

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }
}