#include "TestService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include "../../examples/common/TestMsgJsonSerializer.h"
#include <ichor/optional_bundles/logging_bundle/LoggerAdmin.h>
#include <ichor/optional_bundles/serialization_bundle/SerializationAdmin.h>
#ifdef ICHOR_USE_SPDLOG
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
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        auto logMgr = dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
        logMgr->setLogLevel(LogLevel::INFO);

#ifdef ICHOR_USE_SPDLOG
        dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif

        dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
        dm.createServiceManager<SerializationAdmin, ISerializationAdmin>();
        dm.createServiceManager<TestMsgJsonSerializer, ISerializer>();
        dm.createServiceManager<TestService>();
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
                dm.createServiceManager<SerializationAdmin, ISerializationAdmin>();
                dm.createServiceManager<TestMsgJsonSerializer, ISerializer>();
                dm.createServiceManager<TestService>();
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