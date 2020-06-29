#include "TestService.h"
#include "TrackerService.h"
#include "RuntimeCreatedService.h"
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#include <optional_bundles/logging_bundle/SpdlogLogger.h>
#include <chrono>
#include <iostream>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::system_clock::now();
    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    auto logAdminMgr = dm.createDependencyServiceManager<ILoggerAdmin, LoggerAdmin<SpdlogLogger>>(RequiredList<IFrameworkLogger>, OptionalList<>);
    auto testOneMgr = dm.createDependencyServiceManager<ITestService, TestService>(RequiredList<ILogger, IRuntimeCreatedService>, OptionalList<>, CppelixProperties{{"scope", std::make_shared<Property<std::string>>("one")}});
    auto testTwoMgr = dm.createDependencyServiceManager<ITestService, TestService>(RequiredList<ILogger, IRuntimeCreatedService>, OptionalList<>, CppelixProperties{{"scope", std::make_shared<Property<std::string>>("two")}});
    auto trackerMgr = dm.createDependencyServiceManager<ITrackerService, TrackerService>(RequiredList<ILogger>, OptionalList<>);
    dm.start();
    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Program ran for {:n} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}