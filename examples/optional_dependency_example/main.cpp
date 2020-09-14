#include "TestService.h"
#include "OptionalService.h"
#include "OptionalService.h"
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
#include <chrono>
#include <iostream>

using namespace std::string_literals;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::system_clock::now();
    DependencyManager dm{};
    dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>();
#ifdef USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
    dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
    dm.createServiceManager<OptionalService, IOptionalService>();
    dm.createServiceManager<OptionalService, IOptionalService>();
    dm.createServiceManager<TestService, ITestService>();
    dm.start();
    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}