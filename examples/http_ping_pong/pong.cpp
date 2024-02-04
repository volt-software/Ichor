#include "PongService.h"
#include "PingMsgJsonSerializer.h"
#include "../common/lyra.hpp"
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/network/boost/HttpHostService.h>
#include <ichor/services/network/boost/AsioContextService.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/services/logging/NullLogger.h>

// Some compile time logic to instantiate a regular cout logger or to use the spdlog logger, if Ichor has been compiled with it.
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

#include <chrono>
#include <iostream>

using namespace std::string_literals;
using namespace Ichor;

int main(int argc, char *argv[]) {
    std::locale::global(std::locale("en_US.UTF-8"));

    uint64_t verbosity{};
    uint64_t threads{1};
    bool silent{};
    bool spinlock{};
    bool showHelp{};
    std::string address{"127.0.0.1"};

    auto cli = lyra::help(showHelp)
               | lyra::opt(address, "address")["-a"]["--address"]("Address to bind to, e.g. 127.0.0.1")
               | lyra::opt([&verbosity](bool) { verbosity++; })["-v"]["--verbose"]("Increase logging for each -v").cardinality(0, 4)
               | lyra::opt(threads, "threads")["-t"]["--threads"]("Number of threads to use for I/O, default: 1")
               | lyra::opt(spinlock)["-p"]["--spinlock"]("Spinlock 10ms before going to sleep, improves latency in high workload cases at the expense of CPU usage")
               | lyra::opt(silent)["-s"]["--silent"]("No output");

    auto result = cli.parse( { argc, argv } );
    if (!result) {
        fmt::print("Error in command line: {}\n", result.message());
        return 1;
    }

    if (showHelp) {
        std::cout << cli << "\n";
        return 0;
    }

    Ichor::LogLevel level{Ichor::LogLevel::LOG_ERROR};
    if(verbosity == 1) {
        level = Ichor::LogLevel::LOG_WARN;
    } else if(verbosity == 2) {
        level = Ichor::LogLevel::LOG_INFO;
    } else if(verbosity == 3) {
        level = Ichor::LogLevel::LOG_DEBUG;
    } else if(verbosity >= 4) {
        level = Ichor::LogLevel::LOG_TRACE;
    }

    auto start = std::chrono::steady_clock::now();
    auto queue = std::make_unique<PriorityQueue>(spinlock);
    auto &dm = queue->createManager();

#ifdef ICHOR_USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif

    if(verbosity > 0) {
        dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>(Properties{{"LogLevel", Ichor::make_any<LogLevel>(level)}});
    }

    if(silent) {
        dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
    } else {
        dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(level)}});
    }

    dm.createServiceManager<PingMsgJsonSerializer, ISerializer<PingMsg>>();
    dm.createServiceManager<AsioContextService, IAsioContextService>(Properties{{"Threads", Ichor::make_any<uint64_t>(threads)}});
    dm.createServiceManager<HttpHostService, IHttpHostService>(Properties{{"Address", Ichor::make_any<std::string>(address)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}, {"NoDelay", Ichor::make_any<bool>(true)}});
    dm.createServiceManager<PongService>();
    queue->start(CaptureSigInt);
    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}
