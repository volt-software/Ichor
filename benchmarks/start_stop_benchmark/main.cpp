#include "TestService.h"
#include "StartStopService.h"
#include <cppelix/optional_bundles/logging_bundle/LoggerAdmin.h>
#ifdef USE_SPDLOG
#include <cppelix/optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <cppelix/optional_bundles/logging_bundle/SpdlogLogger.h>

#define FRAMEWORK_LOGGER_TYPE SpdlogFrameworkLogger
#define LOGGER_TYPE SpdlogLogger
#else
#include <cppelix/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <cppelix/optional_bundles/logging_bundle/CoutLogger.h>

#define FRAMEWORK_LOGGER_TYPE CoutFrameworkLogger
#define LOGGER_TYPE CoutLogger
#endif

uint64_t StartStopService::startCount = 0;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>();
    logMgr->setLogLevel(LogLevel::INFO);
#ifdef USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
    dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
    dm.createServiceManager<TestService, ITestService>(CppelixProperties{{"LogLevel", LogLevel::INFO}});
    dm.createServiceManager<StartStopService, IStartStopService>(CppelixProperties{{"LogLevel", LogLevel::INFO}});
    dm.start();

    return 0;
}