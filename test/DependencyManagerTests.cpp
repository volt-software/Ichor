#include "utest.h"
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/NullFrameworkLogger.h>
#include "UselessService.h"

UTEST(DependencyManager, ExceptionOnStart_WhenNoRegistrations) {
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

    ASSERT_TRUE(stopped);
    ASSERT_TRUE(thrown_exception);
}

UTEST(DependencyManager, ExceptionOnStart_WhenNoFrameworkLogger) {
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

    ASSERT_TRUE(stopped);
    ASSERT_TRUE(thrown_exception);
}

UTEST(DependencyManager, QuitOnQuitEvent) {
    Ichor::DependencyManager dm{};

    std::thread t([&]() {
        dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
        dm.createServiceManager<UselessService>();
        dm.start();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    ASSERT_TRUE(dm.isRunning());

    dm.pushEvent<QuitEvent>(0);

    t.join();

    ASSERT_FALSE(dm.isRunning());
}