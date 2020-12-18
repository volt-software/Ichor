#include "TestService.h"
#include <ichor/optional_bundles/logging_bundle/LoggerAdmin.h>
#include <ichor/optional_bundles/logging_bundle/SpdlogSharedService.h>
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
#include <iostream>
#include <thread>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    {
        auto start = std::chrono::system_clock::now();
        DependencyManager dm{};
        auto logMgr = dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>();
        logMgr->setLogLevel(LogLevel::INFO);

#ifdef USE_SPDLOG
        dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif

        dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
        dm.createServiceManager<TestService>(IchorProperties{{"LogLevel", LogLevel::WARN}});
        dm.start();
        auto end = std::chrono::system_clock::now();
        std::cout << fmt::format("Single Threaded program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }

    auto start = std::chrono::system_clock::now();
    std::array<std::thread, 8> threads{};
    std::array<DependencyManager, 8> managers{};
    for(uint_fast32_t i = 0; i < 8; i++) {
        threads[i] = std::thread([&managers, i] {
            auto logMgr = managers[i].createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>();
            logMgr->setLogLevel(LogLevel::INFO);

#ifdef USE_SPDLOG
            managers[i].createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif

            managers[i].createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
            managers[i].createServiceManager<TestService>(IchorProperties{{"LogLevel", LogLevel::WARN}});
            managers[i].start();
        });
    }
    for(uint_fast32_t i = 0; i < 8; i++) {
        threads[i].join();
    }
    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Multi Threaded program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());

    return 0;
}