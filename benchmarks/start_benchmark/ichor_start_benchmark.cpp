#include "TestService.h"
#include "ConstructorInjectionTestService.h"
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/NullLogger.h>
#include <ichor/services/logging/NullFrameworkLogger.h>
#include <ichor/services/metrics/MemoryUsageFunctions.h>
#include <ichor/ichor-mimalloc.h>
#include <iostream>
#include <thread>
#include <array>
#include "../../examples/common/lyra.hpp"

struct GlobalEventStatistics {
    uint64_t occ;
    int64_t min;
    int64_t max;
};

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
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            for (uint64_t i = 0; i < SERVICES_COUNT; i++) {
                dm.createServiceManager<TestService>(Properties{{"Iteration", Ichor::v1::make_any<uint64_t>(i)},
                                                                {"LogLevel",  Ichor::v1::make_any<LogLevel>(LogLevel::LOG_WARN)}});
            }
            queue->start(CaptureSigInt);
            auto end = std::chrono::steady_clock::now();
            fmt::println("{} single threaded advanced injection ran for {:L} µs with {:L} peak memory usage", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
        }

        if(!singleOnly) {
            auto start = std::chrono::steady_clock::now();
            std::array<std::thread, 8> threads{};
            std::array<PriorityQueue, 8> queues{};
            for (uint_fast32_t i = 0, j = 0; i < 8; i++, j += 2) {
                threads[i] = std::thread([&queues, i] {
                    auto &dm = queues[i].createManager();
                    dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
                    dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
                    for (uint64_t z = 0; z < SERVICES_COUNT; z++) {
                        dm.createServiceManager<TestService>(Properties{{"Iteration", Ichor::v1::make_any<uint64_t>(z)}, {"LogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_WARN)}});
                    }
                    queues[i].start(CaptureSigInt);
                });
            }
            for (uint_fast32_t i = 0; i < 8; i++) {
                threads[i].join();
            }
            auto end = std::chrono::steady_clock::now();
            fmt::println("{} multi threaded advanced injection ran for {:L} µs with {:L} peak memory usage",
                            argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
        }
    }

    if(constructerOnly) {
        {
            auto start = std::chrono::steady_clock::now();
            auto queue = std::make_unique<PriorityQueue>();
            auto &dm = queue->createManager();

            unordered_map<uint64_t, GlobalEventStatistics> globalEventStatistics;
            unordered_map<uint64_t, std::string_view> eventTypeToNameMapper;
            std::chrono::time_point<std::chrono::steady_clock> startProcessingTimestamp{};
            auto evtInterceptor = dm.registerGlobalEventInterceptor<Event>([&](Event const &evt) -> bool {
                startProcessingTimestamp = std::chrono::steady_clock::now();

                auto evtType = evt.get_type();
                if(!eventTypeToNameMapper.contains(evtType)) {
                    eventTypeToNameMapper.emplace(evtType, evt.get_name());
                }

                return (bool)AllowOthersHandling;
            }, [&](Event const &evt, bool processed) {
                if(!processed) {
                    return;
                }

                auto now = std::chrono::steady_clock::now();
                auto processingTime = now - startProcessingTimestamp;
                auto evtType = evt.get_type();
                auto statistics = globalEventStatistics.find(evtType);

                if(statistics == end(globalEventStatistics)) {
                    globalEventStatistics.emplace(evtType, GlobalEventStatistics{1, std::chrono::duration_cast<std::chrono::nanoseconds>(processingTime).count(), std::chrono::duration_cast<std::chrono::nanoseconds>(processingTime).count()});
                } else {
                    if(statistics->second.min > std::chrono::duration_cast<std::chrono::nanoseconds>(processingTime).count()) {
                        statistics->second.min = std::chrono::duration_cast<std::chrono::nanoseconds>(processingTime).count();
                    }
                    if(statistics->second.max < std::chrono::duration_cast<std::chrono::nanoseconds>(processingTime).count()) {
                        statistics->second.max = std::chrono::duration_cast<std::chrono::nanoseconds>(processingTime).count();
                    }
                    statistics->second.occ++;
                }
            });

            dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            for (uint64_t i = 0; i < SERVICES_COUNT; i++) {
                dm.createServiceManager<ConstructorInjectionTestService>(Properties{{"Iteration", Ichor::v1::make_any<uint64_t>(i)},
                                                                {"LogLevel",  Ichor::v1::make_any<LogLevel>(LogLevel::LOG_WARN)}});
            }
            queue->start(CaptureSigInt);

            fmt::println("Global Event Statistics:");
            uint64_t total_occ{};
            for(auto &[key, statistics] : globalEventStatistics) {
                fmt::println("Event type {} occurred {:L} times, min/max processing: {:L}/{:L} ns", eventTypeToNameMapper[key], statistics.occ, statistics.min, statistics.max);
                total_occ += statistics.occ;
            }
            fmt::println("total events caught: {}", total_occ);

            auto end = std::chrono::steady_clock::now();
            fmt::println("{} single threaded constructor injection ran for {:L} µs with {:L} peak memory usage", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
        }

        if(!singleOnly) {
            auto start = std::chrono::steady_clock::now();
            std::array<std::thread, 8> threads{};
            std::array<PriorityQueue, 8> queues{};
            for (uint_fast32_t i = 0, j = 0; i < 8; i++, j += 2) {
                threads[i] = std::thread([&queues, i] {
                    auto &dm = queues[i].createManager();
                    dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
                    dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
                    for (uint64_t z = 0; z < SERVICES_COUNT; z++) {
                        dm.createServiceManager<ConstructorInjectionTestService>(Properties{{"Iteration", Ichor::v1::make_any<uint64_t>(z)}, {"LogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_WARN)}});
                    }
                    queues[i].start(CaptureSigInt);
                });
            }
            for (uint_fast32_t i = 0; i < 8; i++) {
                threads[i].join();
            }
            auto end = std::chrono::steady_clock::now();
            fmt::println("{} multi threaded constructor injection ran for {:L} µs with {:L} peak memory usage",
                            argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS());
        }
    }

    return 0;
}
