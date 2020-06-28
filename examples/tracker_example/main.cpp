#include "TestService.h"
#include "TrackerService.h"
#include "RuntimeCreatedService.h"
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <chrono>
#include <iostream>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::system_clock::now();
    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    auto testOneMgr = dm.createDependencyServiceManager<ITestService, TestService>(RequiredList<IFrameworkLogger, IRuntimeCreatedService>, OptionalList<>, CppelixProperties{{"scope", std::make_shared<Property<std::string>>("one")}});
    auto testTwoMgr = dm.createDependencyServiceManager<ITestService, TestService>(RequiredList<IFrameworkLogger, IRuntimeCreatedService>, OptionalList<>, CppelixProperties{{"scope", std::make_shared<Property<std::string>>("two")}});
    auto trackerMgr = dm.createDependencyServiceManager<ITrackerService, TrackerService>(RequiredList<IFrameworkLogger>, OptionalList<>);
    dm.start();
    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Program ran for {:n} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}