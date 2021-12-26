#include <catch2/catch_test_macros.hpp>
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/NullFrameworkLogger.h>
#include "UselessService.h"
#include "spdlog/sinks/stdout_color_sinks.h"

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
                Ichor::DependencyManager dm{};
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
                Ichor::DependencyManager dm{};
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
        Ichor::DependencyManager dm{};

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService>();
            dm.start();
        });

        dm.waitForEmptyQueue();

        REQUIRE(dm.isRunning());

        dm.pushEvent<QuitEvent>(0);

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }
}