#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#include <optional_bundles/metrics_bundle/EventStatisticsService.h>
#include "UsingStatisticsService.h"
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

using namespace Cppelix;
using namespace std::string_literals;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::system_clock::now();
    DependencyManager dm{};
    dm.createServiceManager<IFrameworkLogger, FRAMEWORK_LOGGER_TYPE>();
#ifdef USE_SPDLOG
    dm.createServiceManager<ISpdlogSharedService, SpdlogSharedService>();
#endif
    dm.createServiceManager<ILoggerAdmin, LoggerAdmin<LOGGER_TYPE LOGGER_SHARED_TYPE>>(RequiredList<IFrameworkLogger>);
    dm.createServiceManager<IEventStatisticsService, EventStatisticsService>(RequiredList<ILogger>, OptionalList<>, CppelixProperties{{"ShowStatisticsOnStop", true}});
    dm.createServiceManager<IUsingStatisticsService, UsingStatisticsService>(RequiredList<ILogger>, OptionalList<>);
    dm.start();
    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}