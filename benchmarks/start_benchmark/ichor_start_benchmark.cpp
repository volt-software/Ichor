#include "TestService.h"
#include "ConstructorInjectionTestService.h"
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/NullLogger.h>
#include <ichor/services/metrics/MemoryUsageFunctions.h>
#include <iostream>
#include <thread>
#include <array>
#include "../../examples/common/lyra.hpp"

int main(int argc, char *argv[]) {
    try {
        std::locale::global(std::locale("en_US.UTF-8"));
    } catch(std::runtime_error const &e) {
        fmt::println("Couldn't set locale to en_US.UTF-8: {}", e.what());
    }

    bool showHelp{};
    bool singleOnly{};
    bool advancedOnly{};
    bool constructerOnly{};

    auto cli = lyra::help(showHelp)
               | lyra::opt(singleOnly)["-s"]["--single"]("Single core only")
               | lyra::opt(advancedOnly)["-a"]["--advanced"]("Advanced Service only")
               | lyra::opt(constructerOnly)["-c"]["--constructor"]("Constructor Injection only");

    auto result = cli.parse( { argc, argv } );
    if (!result) {
        fmt::print("Error in command line: {}\n", result.message());
        return 1;
    }

    if (showHelp) {
        std::cout << cli << "\n";
        return 0;
    }

    if(!advancedOnly && !constructerOnly) {
        advancedOnly = true;
        constructerOnly = true;
    }


    if(advancedOnly) {
        {
            auto start = std::chrono::steady_clock::now();
            auto queue = std::make_unique<PriorityQueue>();
            auto &dm = queue->createManager();
            dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
            for (uint64_t i = 0; i < SERVICES_COUNT; i++) {
                dm.createServiceManager<TestService>(Properties{{"Iteration", Ichor::make_any<uint64_t>(i)},
                                                                {"LogLevel",  Ichor::make_any<LogLevel>(LogLevel::LOG_WARN)}});
            }
            queue->start(CaptureSigInt);
            auto end = std::chrono::steady_clock::now();
            std::cout << fmt::format("{} single threaded advanced injection ran for {:L} µs with {:L} peak memory usage\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
        }

        if(!singleOnly) {
            auto start = std::chrono::steady_clock::now();
            std::array<std::thread, 8> threads{};
            std::array<PriorityQueue, 8> queues{};
            for (uint_fast32_t i = 0, j = 0; i < 8; i++, j += 2) {
                threads[i] = std::thread([&queues, i] {
                    auto &dm = queues[i].createManager();
                    dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
                    for (uint64_t z = 0; z < SERVICES_COUNT; z++) {
                        dm.createServiceManager<TestService>(Properties{{"Iteration", Ichor::make_any<uint64_t>(z)}, {"LogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_WARN)}});
                    }
                    queues[i].start(CaptureSigInt);
                });
            }
            for (uint_fast32_t i = 0; i < 8; i++) {
                threads[i].join();
            }
            auto end = std::chrono::steady_clock::now();
            std::cout << fmt::format("{} multi threaded advanced injection ran for {:L} µs with {:L} peak memory usage\n",
                                     argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
        }
    }

    if(constructerOnly) {
        {
            auto start = std::chrono::steady_clock::now();
            auto queue = std::make_unique<PriorityQueue>();
            auto &dm = queue->createManager();
            dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
            for (uint64_t i = 0; i < SERVICES_COUNT; i++) {
                dm.createServiceManager<ConstructorInjectionTestService>(Properties{{"Iteration", Ichor::make_any<uint64_t>(i)},
                                                                {"LogLevel",  Ichor::make_any<LogLevel>(LogLevel::LOG_WARN)}});
            }
            queue->start(CaptureSigInt);
            auto end = std::chrono::steady_clock::now();
            std::cout << fmt::format("{} single threaded constructor injection ran for {:L} µs with {:L} peak memory usage\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
        }

        if(!singleOnly) {
            auto start = std::chrono::steady_clock::now();
            std::array<std::thread, 8> threads{};
            std::array<PriorityQueue, 8> queues{};
            for (uint_fast32_t i = 0, j = 0; i < 8; i++, j += 2) {
                threads[i] = std::thread([&queues, i] {
                    auto &dm = queues[i].createManager();
                    dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
                    for (uint64_t z = 0; z < SERVICES_COUNT; z++) {
                        dm.createServiceManager<ConstructorInjectionTestService>(Properties{{"Iteration", Ichor::make_any<uint64_t>(z)}, {"LogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_WARN)}});
                    }
                    queues[i].start(CaptureSigInt);
                });
            }
            for (uint_fast32_t i = 0; i < 8; i++) {
                threads[i].join();
            }
            auto end = std::chrono::steady_clock::now();
            std::cout << fmt::format("{} multi threaded constructor injection ran for {:L} µs with {:L} peak memory usage\n",
                                     argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
        }
    }

    return 0;
}
