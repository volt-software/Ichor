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
    logMgr->setLogLevel(LogLevel::INFO);
    auto logAdminMgr = dm.createServiceManager<ILoggerAdmin, LoggerAdmin<SpdlogLogger>>();
    auto testMgr = dm.createServiceManager<ITestService, TestService>(RequiredList<ILogger>, OptionalList<>, CppelixProperties{{"LogLevel", LogLevel::INFO}});
    auto startStopMgr = dm.createServiceManager<IStartStopService, StartStopService>(RequiredList<ILogger, ITestService>, OptionalList<>, CppelixProperties{{"LogLevel", LogLevel::INFO}});
    dm.start();

    return 0;
}