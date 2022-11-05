#include "TestService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerAdmin.h>
#include <ichor/services/logging/NullFrameworkLogger.h>
#include <ichor/services/logging/NullLogger.h>
#include <ichor/services/metrics/MemoryUsageFunctions.h>
#include <iostream>

int main(int argc, char *argv[]) {
    std::locale::global(std::locale("en_US.UTF-8"));
    std::ios::sync_with_stdio(false);

    {
        auto start = std::chrono::steady_clock::now();
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>({}, 10);
        dm.createServiceManager<LoggerAdmin<NullLogger>, ILoggerAdmin>();
        dm.createServiceManager<TestService>(Properties{{"LogLevel", Ichor::make_any<LogLevel>(LogLevel::WARN)}});
        queue->start(CaptureSigInt);
        auto end = std::chrono::steady_clock::now();
        std::cout << fmt::format("{} single threaded ran for {:L} µs with {:L} peak memory usage\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
    }

    {
        auto start = std::chrono::steady_clock::now();
        std::array<std::thread, 8> threads{};
        std::array<MultimapQueue, 8> queues{};
        for (uint_fast32_t i = 0, j = 0; i < 8; i++, j += 2) {
            threads[i] = std::thread([&queues, i] {
                auto &dm = queues[i].createManager();
                dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>({}, 10);
                dm.createServiceManager<LoggerAdmin<NullLogger>, ILoggerAdmin>();
                dm.createServiceManager<TestService>(Properties{{"LogLevel", Ichor::make_any<LogLevel>(LogLevel::WARN)}});
                queues[i].start(CaptureSigInt);
            });
        }
        for (uint_fast32_t i = 0; i < 8; i++) {
            threads[i].join();
        }
        auto end = std::chrono::steady_clock::now();
        std::cout << fmt::format("{} multi threaded ran for {:L} µs with {:L} peak memory usage\n",
                                 argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
    }

    return 0;
}