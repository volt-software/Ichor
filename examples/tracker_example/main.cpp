#include "TestService.h"
#include "TrackerService.h"
#include "RuntimeCreatedService.h"
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#ifdef USE_SPDLOG
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <optional_bundles/logging_bundle/SpdlogLogger.h>

#define FRAMEWORK_LOGGER_TYPE SpdlogFrameworkLogger
#define LOGGER_TYPE SpdlogLogger
#else
#include <optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <optional_bundles/logging_bundle/CoutLogger.h>

#define FRAMEWORK_LOGGER_TYPE CoutFrameworkLogger
#define LOGGER_TYPE CoutLogger
#endif
#include <chrono>
#include <iostream>

using namespace std::string_literals;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::system_clock::now();
    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, FRAMEWORK_LOGGER_TYPE>();
    auto logAdminMgr = dm.createServiceManager<ILoggerAdmin, LoggerAdmin<LOGGER_TYPE>>(RequiredList<IFrameworkLogger>, OptionalList<>);
    auto testOneMgr = dm.createServiceManager<ITestService, TestService>(RequiredList<ILogger, IRuntimeCreatedService>, OptionalList<>, CppelixProperties{{"scope", "one"s}});
    auto testTwoMgr = dm.createServiceManager<ITestService, TestService>(RequiredList<ILogger, IRuntimeCreatedService>, OptionalList<>, CppelixProperties{{"scope", "two"s}});
    auto trackerMgr = dm.createServiceManager<ITrackerService, TrackerService>(RequiredList<ILogger>, OptionalList<>);
    dm.start();
    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}