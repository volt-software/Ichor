#include "UsingTimerService.h"
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#ifdef USE_SPDLOG
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <optional_bundles/logging_bundle/SpdlogLogger.h>

#define FRAMEWORK_LOGGER_TYPE SpdlogFrameworkLogger
#define LOGGER_TYPE SpdlogLogger
#define LOGGER_SHARED_TYPE ,ISpdlogSharedService
#else
#include <optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <optional_bundles/logging_bundle/CoutLogger.h>

#define FRAMEWORK_LOGGER_TYPE CoutFrameworkLogger
#define LOGGER_TYPE CoutLogger
#define LOGGER_SHARED_TYPE
#endif
#include <chrono>
#include <iostream>

using namespace std::string_literals;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::system_clock::now();
    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, FRAMEWORK_LOGGER_TYPE>();
#ifdef USE_SPDLOG
    dm.createServiceManager<ISpdlogSharedService, SpdlogSharedService>();
#endif
    dm.createServiceManager<ILoggerAdmin, LoggerAdmin<LOGGER_TYPE LOGGER_SHARED_TYPE>>(RequiredList<IFrameworkLogger>);
    dm.createServiceManager<IUsingTimerService, UsingTimerService>(RequiredList<ILogger>, OptionalList<>);
    dm.createServiceManager<IUsingTimerService, UsingTimerService>(RequiredList<ILogger>, OptionalList<>);
    dm.start();
    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}