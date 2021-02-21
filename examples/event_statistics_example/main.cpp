#include <ichor/optional_bundles/logging_bundle/LoggerAdmin.h>
#include <ichor/optional_bundles/metrics_bundle/EventStatisticsService.h>
#include "UsingStatisticsService.h"
#ifdef USE_SPDLOG
#include <ichor/optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <ichor/optional_bundles/logging_bundle/SpdlogLogger.h>

#define FRAMEWORK_LOGGER_TYPE SpdlogFrameworkLogger
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <ichor/optional_bundles/logging_bundle/CoutLogger.h>

#define FRAMEWORK_LOGGER_TYPE CoutFrameworkLogger
#define LOGGER_TYPE CoutLogger
#endif
#include <chrono>
#include <iostream>

using namespace Ichor;
using namespace std::string_literals;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::steady_clock::now();
    DependencyManager dm{};
    dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
#ifdef USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
    dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
    dm.createServiceManager<EventStatisticsService, IEventStatisticsService>(IchorProperties{{"ShowStatisticsOnStop", make_any<bool>(dm.getMemoryResource(), true)}});
    dm.createServiceManager<UsingStatisticsService>();
    dm.start();
    auto end = std::chrono::steady_clock::now();
    fmt::print("Program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}