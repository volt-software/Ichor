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
#include <ichor/optional_bundles/metrics_bundle/MemoryUsageFunctions.h>
#include <iostream>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    {
        auto start = std::chrono::steady_clock::now();
        std::pmr::unsynchronized_pool_resource resourceOne{};
        std::pmr::unsynchronized_pool_resource resourceTwo{};
        DependencyManager dm{&resourceOne, &resourceTwo};
        auto logMgr = dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
        logMgr->setLogLevel(LogLevel::INFO);

#ifdef USE_SPDLOG
        dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif

        dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
        dm.createServiceManager<TestService>(Properties{{"LogLevel", Ichor::make_any<LogLevel>(dm.getMemoryResource(), LogLevel::WARN)}});
        dm.start();
        auto end = std::chrono::steady_clock::now();
        std::cout << fmt::format("Single Threaded Program ran for {:L} µs with {:L} peak memory usage\n", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
    }

    std::array<std::pmr::unsynchronized_pool_resource, 16> memoryAllocators{};
    {
        auto start = std::chrono::steady_clock::now();
        std::array<std::thread, 8> threads{};
        std::vector<DependencyManager> managers{};
        managers.reserve(8);
        for (uint_fast32_t i = 0, j = 0; i < 8; i++, j += 2) {
            managers.emplace_back(&memoryAllocators[j], &memoryAllocators[j + 1]);
            threads[i] = std::thread([&managers, i] {
                auto logMgr = managers[i].createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
                logMgr->setLogLevel(LogLevel::INFO);

#ifdef USE_SPDLOG
                managers[i].createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif

                managers[i].createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
                managers[i].createServiceManager<TestService>(Properties{{"LogLevel", Ichor::make_any<LogLevel>(managers[i].getMemoryResource(), LogLevel::WARN)}});
                managers[i].start();
            });
        }
        for (uint_fast32_t i = 0; i < 8; i++) {
            threads[i].join();
        }
        auto end = std::chrono::steady_clock::now();
        std::cout << fmt::format("Multi Threaded program ran for {:L} µs with {:L} peak memory usage\n",
                                 std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
    }

    return 0;
}