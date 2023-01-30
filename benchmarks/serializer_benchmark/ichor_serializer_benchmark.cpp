#include "TestService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/services/logging/NullLogger.h>
#include <ichor/services/metrics/MemoryUsageFunctions.h>
#include <iostream>
#include <thread>
#include <array>
#include "../../examples/common/TestMsgRapidJsonSerializer.h"
#include "../../examples/common/TestMsgBoostJsonSerializer.h"
#include "../../examples/common/lyra.hpp"

uint64_t sizeof_test{};
const uint32_t threadCount{8};

int main(int argc, char *argv[]) {
    std::locale::global(std::locale("en_US.UTF-8"));

    bool showHelp{};
    bool singleOnly{};
    bool rapidjsonOnly{};
    [[maybe_unused]] bool boostjsonOnly{};

    auto cli = lyra::help(showHelp)
               | lyra::opt(rapidjsonOnly)["-r"]["--rapid-json"]("enable only rapid json")
#ifdef ICHOR_USE_BOOST_JSON
               | lyra::opt(boostjsonOnly)["-b"]["--boost-json"]("enable only boost json")
#endif
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

    if((!rapidjsonOnly && !boostjsonOnly) || rapidjsonOnly)
    {
        {
            auto start = std::chrono::steady_clock::now();
            auto queue = std::make_unique<MultimapQueue>();
            auto &dm = queue->createManager();
            dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
            dm.createServiceManager<TestMsgRapidJsonSerializer, ISerializer<TestMsg>>();
            dm.createServiceManager<TestService>();
            queue->start(CaptureSigInt);
            auto end = std::chrono::steady_clock::now();
            std::cout << fmt::format("{} single threaded rapidjson ran for {:L} µs with {:L} peak memory usage {:L} MB/s\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS(),
                                     std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * SERDE_COUNT * static_cast<double>(sizeof_test) / 1'000'000.));
        }

        if(!singleOnly) {
            auto start = std::chrono::steady_clock::now();
            std::array<std::thread, threadCount> threads{};
            std::array<MultimapQueue, threadCount> queues{};
            for (uint_fast32_t i = 0, j = 0; i < threadCount; i++, j += 2) {
                threads[i] = std::thread([&queues, i] {
                    auto &dm = queues[i].createManager();
                    dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
                    dm.createServiceManager<TestMsgRapidJsonSerializer, ISerializer<TestMsg>>();
                    dm.createServiceManager<TestService>();
                    queues[i].start(CaptureSigInt);
                });
            }
            for (uint_fast32_t i = 0; i < threadCount; i++) {
                threads[i].join();
            }
            auto end = std::chrono::steady_clock::now();
            std::cout << fmt::format("{} multi threaded rapidjson ran for {:L} µs with {:L} peak memory usage {:L} MB/s\n",
                                     argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS(),
                                     std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * SERDE_COUNT * threadCount * static_cast<double>(sizeof_test) / 1'000'000.));
        }
    }

#ifdef ICHOR_USE_BOOST_JSON
    if((!rapidjsonOnly && !boostjsonOnly) || boostjsonOnly)
    {
        {
            auto start = std::chrono::steady_clock::now();
            auto queue = std::make_unique<MultimapQueue>();
            auto &dm = queue->createManager();
            dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
            dm.createServiceManager<TestMsgBoostJsonSerializer, ISerializer<TestMsg>>();
            dm.createServiceManager<TestService>();
            queue->start(CaptureSigInt);
            auto end = std::chrono::steady_clock::now();
            std::cout << fmt::format("{} single threaded boost.JSON ran for {:L} µs with {:L} peak memory usage {:L} MB/s\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS(),
                                     std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * SERDE_COUNT * static_cast<double>(sizeof_test) / 1'000'000.));
        }

        if(!singleOnly) {
            auto start = std::chrono::steady_clock::now();
            std::array<std::thread, threadCount> threads{};
            std::array<MultimapQueue, threadCount> queues{};
            for (uint_fast32_t i = 0, j = 0; i < threadCount; i++, j += 2) {
                threads[i] = std::thread([&queues, i] {
                    auto &dm = queues[i].createManager();
                    dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
                    dm.createServiceManager<TestMsgBoostJsonSerializer, ISerializer<TestMsg>>();
                    dm.createServiceManager<TestService>();
                    queues[i].start(CaptureSigInt);
                });
            }
            for (uint_fast32_t i = 0; i < threadCount; i++) {
                threads[i].join();
            }
            auto end = std::chrono::steady_clock::now();
            std::cout << fmt::format("{} multi threaded boost.JSON ran for {:L} µs with {:L} peak memory usage {:L} MB/s\n",
                                     argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS(),
                                     std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * SERDE_COUNT * threadCount * static_cast<double>(sizeof_test) / 1'000'000.));
        }
    }
#endif

    return 0;
}
