#include "TestService.h"
#include "StartStopService.h"
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#include <optional_bundles/logging_bundle/SpdlogLogger.h>

uint64_t StartStopService::startCount = 0;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    spdlog::set_level(spdlog::level::info);
    auto logAdminMgr = dm.createServiceManager<ILoggerAdmin, LoggerAdmin<SpdlogLogger>>();
    auto testMgr = dm.createDependencyServiceManager<ITestService, TestService>(RequiredList<ILogger>, OptionalList<>);
    auto startStopMgr = dm.createDependencyServiceManager<IStartStopService, StartStopService>(RequiredList<ILogger, ITestService>, OptionalList<>);
    dm.start();

    return 0;
}