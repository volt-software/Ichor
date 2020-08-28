#include "TestService.h"
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#include <optional_bundles/logging_bundle/SpdlogSharedService.h>
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
#include <iostream>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::system_clock::now();
    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, FRAMEWORK_LOGGER_TYPE>();
    logMgr->setLogLevel(LogLevel::INFO);


#ifdef USE_SPDLOG
    dm.createServiceManager<ISpdlogSharedService, SpdlogSharedService>();
#endif

    dm.createServiceManager<ILoggerAdmin, LoggerAdmin<LOGGER_TYPE LOGGER_SHARED_TYPE>>(RequiredList<IFrameworkLogger>);
    for(uint64_t i = 0; i < 10'000; i++) {
        dm.createServiceManager<ITestService, TestService>(RequiredList<ILogger>, OptionalList<>, CppelixProperties{{"Iteration", i}, {"LogLevel", LogLevel::WARN}});
    }
    dm.start();
    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}