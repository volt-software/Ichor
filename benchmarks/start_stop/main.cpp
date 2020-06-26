#include "TestService.h"
#include "StartStopService.h"
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>

uint64_t StartStopService::startCount = 0;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    Framework fw{{}};
    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    spdlog::set_level(spdlog::level::info);
    auto testMgr = dm.createDependencyServiceManager<ITestService, TestService>(RequiredList<IFrameworkLogger>, OptionalList<>);
    auto startStopMgr = dm.createDependencyServiceManager<IStartStopService, StartStopService>(RequiredList<IFrameworkLogger, ITestService>, OptionalList<>);
    dm.start();

    return 0;
}