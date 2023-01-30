#include "UsingWsService.h"
#include "../common/TestMsgRapidJsonSerializer.h"
#include "../common/lyra.hpp"
#include <ichor/services/logging/NullLogger.h>
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/network/ws/WsHostService.h>
#include <ichor/services/network/ws/WsConnectionService.h>
#include <ichor/services/network/ClientAdmin.h>
#include <ichor/services/serialization/ISerializer.h>

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

template <typename Logger>
void setLevel(uint64_t verbosity, Logger *logger) {
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

    if constexpr(Derived<Logger, IFrameworkLogger>) {
        logger->setLogLevel(level);
    } else {
        logger->setDefaultLogLevel(level);
    }
}

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

    auto start = std::chrono::steady_clock::now();
    auto queue = std::make_unique<MultimapQueue>(spinlock);
    auto &dm = queue->createManager();
#ifdef ICHOR_USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif

    if(verbosity > 0) {
        auto *logger = dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>();
        setLevel(verbosity, logger->getImplementation());
    }

    if(silent) {
        dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
    } else {
        auto *admin = dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>();
        setLevel(verbosity, admin->getImplementation());
    }
    dm.createServiceManager<TestMsgRapidJsonSerializer, ISerializer<TestMsg>>();
    dm.createServiceManager<AsioContextService, IAsioContextService>(Properties{{"Threads", Ichor::make_any<uint64_t>(threads)}});
    dm.createServiceManager<WsHostService, IHostService>(Properties{{"Address", Ichor::make_any<std::string>(address)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
    dm.createServiceManager<ClientAdmin<WsConnectionService>, IClientAdmin>();
    dm.createServiceManager<UsingWsService>(Properties{{"Address", Ichor::make_any<std::string>(address)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
    queue->start(CaptureSigInt);
    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}
