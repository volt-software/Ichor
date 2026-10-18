#include "TestService.h"
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/services/logging/NullLogger.h>
#include <ichor/services/logging/NullFrameworkLogger.h>
#include <ichor/services/metrics/MemoryUsageFunctions.h>
#include <ichor/ichor-mimalloc.h>
#include <algorithm>
#include <iostream>
#include <thread>
#include <array>
#include <algorithm>
#include "../../examples/common/TestMsgGlazeSerializer.h"
#include "../../examples/common/lyra.hpp"

uint64_t sizeof_test{};
const uint32_t threadCount{8};

int main(int argc, char *argv[]) {
#if ICHOR_EXCEPTIONS_ENABLED
    try {
#endif
        std::locale::global(std::locale("en_US.UTF-8"));
#if ICHOR_EXCEPTIONS_ENABLED
    } catch(std::runtime_error const &e) {
        fmt::println("Couldn't set locale to en_US.UTF-8: {}", e.what());
    }
#endif

    bool showHelp{};
    bool singleOnly{};

    auto cli = lyra::help(showHelp)
               | lyra::opt(singleOnly)["-s"]["--single"]("Single core only");

    auto result = cli.parse( { argc, argv } );
    if (!result) {
        fmt::print("Error in command line: {}\n", result.message());
        return 1;
    }

    if (showHelp) {
        std::cout << cli << "\n";
        return 0;
    }

    {
        auto start = std::chrono::steady_clock::now();
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->getManager();
        dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
        dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
        dm.createServiceManager<TestMsgGlazeSerializer, ISerializer<TestMsg>>();
        dm.createServiceManager<TestService>();
        queue->start(CaptureSigInt);
        auto end = std::chrono::steady_clock::now();
        fmt::println("{} single threaded glaze ran for {:L} µs with {:L} peak memory usage {:L} MB/s", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS(),
                     std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * SERDE_COUNT * static_cast<double>(sizeof_test) / 1'000'000.));
    }

    if(!singleOnly) {
        std::array<std::thread, threadCount> threads{};
        std::array<int64_t, threadCount> threadDurations{};
        for (uint_fast32_t i = 0, j = 0; i < threadCount; i++, j += 2) {
            threads[i] = std::thread([&threadDurations, i] {
                auto start = std::chrono::steady_clock::now();
                PriorityQueue q;
                auto &dm = q.getManager();
                dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
                dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
                dm.createServiceManager<TestMsgGlazeSerializer, ISerializer<TestMsg>>();
                dm.createServiceManager<TestService>();
                q.start(CaptureSigInt);
                auto end = std::chrono::steady_clock::now();
                threadDurations[i] = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            });
        }
        for (uint_fast32_t i = 0; i < threadCount; i++) {
            threads[i].join();
        }
        // Report the median thread instead of the wall time of all threads. How fast a thread runs depends on where its
        // stack and heap end up in memory (e.g. Zen 4 L1D/store forwarding address hashes colliding), which is random
        // per thread and per run. The wall time is that of the slowest thread, which made the results bimodal.
        std::ranges::sort(threadDurations);
        auto median = (threadDurations[threadCount / 2 - 1] + threadDurations[threadCount / 2]) / 2;
        fmt::println("{} multi threaded glaze ran for {:L} µs (median thread, fastest {:L} µs, slowest {:L} µs) with {:L} peak memory usage {:L} MB/s",
                     argv[0], median, threadDurations.front(), threadDurations.back(), getPeakRSS(),
                     std::floor(1'000'000. / static_cast<double>(median) * SERDE_COUNT * threadCount * static_cast<double>(sizeof_test) / 1'000'000.));
    }

    return 0;
}
