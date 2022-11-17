#include "TestService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include "../../examples/common/TestMsgJsonSerializer.h"
#include <ichor/services/logging/LoggerAdmin.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/services/logging/NullLogger.h>
#include <ichor/services/metrics/MemoryUsageFunctions.h>
#include <iostream>
#include <thread>
#include <array>

uint64_t sizeof_test{};

int main(int argc, char *argv[]) {
    std::locale::global(std::locale("en_US.UTF-8"));

    {
        auto start = std::chrono::steady_clock::now();
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        dm.createServiceManager<LoggerAdmin<NullLogger>, ILoggerAdmin>();
        dm.createServiceManager<TestMsgJsonSerializer, ISerializer<TestMsg>>();
        dm.createServiceManager<TestService>();
        queue->start(CaptureSigInt);
        auto end = std::chrono::steady_clock::now();
        std::cout << fmt::format("{} single threaded ran for {:L} µs with {:L} peak memory usage {:L} B/s\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS(),
                                 std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * SERDE_COUNT * static_cast<double>(sizeof_test)));
    }

    {
        auto start = std::chrono::steady_clock::now();
        std::array<std::thread, 8> threads{};
        std::array<MultimapQueue, 8> queues{};
        for (uint_fast32_t i = 0, j = 0; i < 8; i++, j += 2) {
            threads[i] = std::thread([&queues, i] {
                auto &dm = queues[i].createManager();
                dm.createServiceManager<LoggerAdmin<NullLogger>, ILoggerAdmin>();
                dm.createServiceManager<TestMsgJsonSerializer, ISerializer<TestMsg>>();
                dm.createServiceManager<TestService>();
                queues[i].start(CaptureSigInt);
            });
        }
        for (uint_fast32_t i = 0; i < 8; i++) {
            threads[i].join();
        }
        auto end = std::chrono::steady_clock::now();
        std::cout << fmt::format("{} multi threaded ran for {:L} µs with {:L} peak memory usage {:L} B/s\n",
                                 argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS(),
                                 std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * SERDE_COUNT * 8. * static_cast<double>(sizeof_test)));
    }

    return 0;
}