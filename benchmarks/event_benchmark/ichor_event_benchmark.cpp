#include "TestService.h"
#include <ichor/event_queues/PriorityQueue.h>
#ifdef ICHOR_USE_LIBURING
#include <ichor/event_queues/IOUringQueue.h>
#endif
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/NullFrameworkLogger.h>
#include <ichor/services/logging/NullLogger.h>
#include <ichor/services/metrics/MemoryUsageFunctions.h>
#include <ichor/ichor-mimalloc.h>
#include <iostream>
#include <thread>
#include <array>
#include "../../examples/common/lyra.hpp"
//#include <spdlog/spdlog.h>
//#include <spdlog/sinks/stdout_color_sinks.h>

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
//    auto new_logger = spdlog::stdout_color_mt("default_logger");
//    new_logger->set_pattern("[%H:%M:%S.%f] [%t] [%l] %v");
//    spdlog::set_default_logger(new_logger);

    bool showHelp{};
    bool singleOnly{};
    bool liburing{};

    auto cli = lyra::help(showHelp)
#ifdef ICHOR_USE_LIBURING
               | lyra::opt(liburing)["-u"]["--liburing"]("Use io_uring as a queue")
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

    {
        auto start = std::chrono::steady_clock::now();
        std::unique_ptr<IEventQueue> queue;
        if(liburing) {
#ifdef ICHOR_USE_LIBURING
            auto q = std::make_unique<IOUringQueue>(10, 10'000);
            if(!q->createEventLoop()) {
                fmt::println("Couldn't create event loop.");
                std::terminate();
            }
            queue = std::move(q);
#endif
        } else {
            queue = std::make_unique<PriorityQueue>();
        }
//        spdlog::info("start");
        auto &dm = queue->createManager();
        dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
        dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
        dm.createServiceManager<TestService>(Properties{{"LogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_WARN)}});
        queue->start(CaptureSigInt);
        auto end = std::chrono::steady_clock::now();
        fmt::println("{} single threaded ran for {:L} µs with {:L} peak memory usage {:L} events/s",argv[0],  std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS(),
                     std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * EVENT_COUNT));
    }

    if(!singleOnly) {
        auto start = std::chrono::steady_clock::now();
        std::array<std::thread, 8> threads{};
        for (uint_fast32_t i = 0, j = 0; i < 8; i++, j += 2) {
            threads[i] = std::thread([liburing] {
//                spdlog::info("start");
                std::unique_ptr<IEventQueue> queue;
                if(liburing) {
#ifdef ICHOR_USE_LIBURING
                    auto q = std::make_unique<IOUringQueue>(10, 10'000);
                    if(!q->createEventLoop()) {
                        fmt::println("Couldn't create event loop.");
                        std::terminate();
                    }
                    queue = std::move(q);
#endif
                } else {
                    queue = std::make_unique<PriorityQueue>();
                }
                auto &dm = queue->createManager();
                dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
                dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
                dm.createServiceManager<TestService>(Properties{{"LogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_WARN)}});
                queue->start(CaptureSigInt);
            });
        }
        for (uint_fast32_t i = 0; i < 8; i++) {
            threads[i].join();
        }
        auto end = std::chrono::steady_clock::now();
        fmt::println("{} multi threaded ran for {:L} µs with {:L} peak memory usage {:L} events/s",
                     argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), getPeakRSS(),
                     std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * EVENT_COUNT * 8.));
    }

    return 0;
}
