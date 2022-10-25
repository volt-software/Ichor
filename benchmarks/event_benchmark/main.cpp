#include "TestService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerAdmin.h>
#include <ichor/services/logging/SpdlogSharedService.h>
#ifdef ICHOR_USE_SPDLOG
#include <ichor/services/logging/SpdlogFrameworkLogger.h>
#include <ichor/services/logging/SpdlogLogger.h>

#define FRAMEWORK_LOGGER_TYPE SpdlogFrameworkLogger
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <ichor/services/logging/CoutLogger.h>

#define FRAMEWORK_LOGGER_TYPE CoutFrameworkLogger
#define LOGGER_TYPE CoutLogger
#endif
#include <ichor/services/metrics/MemoryUsageFunctions.h>
#include <iostream>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    {
        auto start = std::chrono::steady_clock::now();
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        auto logMgr = dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
        logMgr->setLogLevel(LogLevel::INFO);

#ifdef ICHOR_USE_SPDLOG
        dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif

        dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
        dm.createServiceManager<TestService>(Properties{{"LogLevel", Ichor::make_any<LogLevel>(LogLevel::WARN)}});
        queue->start(CaptureSigInt);
        auto end = std::chrono::steady_clock::now();
        std::cout << fmt::format("Single Threaded Program ran for {:L} µs with {:L} peak memory usage\n", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
    }

    {
        auto start = std::chrono::steady_clock::now();
        std::array<std::thread, 8> threads{};
        std::array<MultimapQueue, 8> queues{};
        for (uint_fast32_t i = 0, j = 0; i < 8; i++, j += 2) {
            threads[i] = std::thread([&queues, i] {
                auto &dm = queues[i].createManager();
                auto logMgr = dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
                logMgr->setLogLevel(LogLevel::INFO);

#ifdef ICHOR_USE_SPDLOG
                dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif

                dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
                dm.createServiceManager<TestService>(Properties{{"LogLevel", Ichor::make_any<LogLevel>(LogLevel::WARN)}});
                queues[i].start(CaptureSigInt);
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