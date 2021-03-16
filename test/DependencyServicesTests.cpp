#include "utest.h"
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/NullFrameworkLogger.h>
#include "UselessService.h"
#include "QuitOnStartWithDependenciesService.h"

UTEST(DependencyServices, QuitOnQuitEvent) {
    Ichor::DependencyManager dm{};

    std::thread t([&]() {
        dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
        dm.createServiceManager<UselessService>();
        dm.createServiceManager<QuitOnStartWithDependenciesService>();
        dm.start();
    });

    t.join();

    ASSERT_FALSE(dm.isRunning());
}