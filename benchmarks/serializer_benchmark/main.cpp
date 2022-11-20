#include "TestService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerAdmin.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/services/logging/NullLogger.h>
#include <ichor/services/metrics/MemoryUsageFunctions.h>
#include <iostream>
#include <thread>
#include <array>
#include "../../examples/common/TestMsgJsonSerializer.h"
#include "../../examples/common/lyra.hpp"

uint64_t sizeof_test{};

int main(int argc, char *argv[]) {
    std::locale::global(std::locale("en_US.UTF-8"));

    bool showHelp{};
    bool singleOnly{};

    auto cli = lyra::help(showHelp)
               | lyra::opt(singleOnly)["-s"]["--single"]("Single core only");

    auto result = cli.parse( { argc, argv } );
    if (!result) {
        fmt::print("Error in command line: {}\n", result.message());
        return 1;
    }

    if (showHelp)
    {
        std::cout << cli << "\n";
        return 0;
    }

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

    if(!singleOnly) {
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