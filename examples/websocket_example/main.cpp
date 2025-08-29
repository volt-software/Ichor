#include "UsingWsService.h"
#include "../common/TestMsgGlazeSerializer.h"
#include "../common/lyra.hpp"
#include <ichor/services/logging/NullLogger.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/network/ClientFactory.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/ichor-mimalloc.h>

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

#if defined(URING_EXAMPLE)
#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/services/network/tcp/IOUringTcpConnectionService.h>
#include <ichor/services/network/tcp/IOUringTcpHostService.h>

#define QIMPL IOUringQueue
#define CONNIMPL IOUringTcpConnectionService
#define HOSTIMPL IOUringTcpHostService
#define WSHOSTIMPL
#define WSCONNIMPL
#else
#include <ichor/services/network/boost/WsHostService.h>
#include <ichor/services/network/boost/WsConnectionService.h>
#include <ichor/event_queues/BoostAsioQueue.h>

#define QIMPL BoostAsioQueue
#define WSHOSTIMPL Boost::v1::WsHostService
#define WSCONNIMPL Boost::v1::WsConnectionService
#endif

#include <chrono>
#include <iostream>

using namespace std::string_literals;
using namespace Ichor;

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

    uint64_t verbosity{};
    bool silent{};
    bool showHelp{};
    std::string address{"127.0.0.1"};

    auto cli = lyra::help(showHelp)
               | lyra::opt(address, "address")["-a"]["--address"]("Address to bind to, e.g. 127.0.0.1")
               | lyra::opt([&verbosity](bool) { verbosity++; })["-v"]["--verbose"]("Increase logging for each -v").cardinality(0, 4)
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
    uint64_t priorityToEnsureHostStartingFirst = 51;

    auto start = std::chrono::steady_clock::now();
    auto queue = std::make_unique<QIMPL>(500);
    auto &dm = queue->createManager();
#ifdef URING_EXAMPLE
    if(!queue->createEventLoop()) {
        fmt::println("Couldn't create io_uring event loop");
        return -1;
    }
#endif
#ifdef ICHOR_USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>(Properties{}, priorityToEnsureHostStartingFirst);
#endif

    dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>(Properties{{"LogLevel", Ichor::v1::make_any<LogLevel>(level)}}, priorityToEnsureHostStartingFirst);

    if(silent) {
        dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>(Properties{}, priorityToEnsureHostStartingFirst);
    } else {
        dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(level)}}, priorityToEnsureHostStartingFirst);
    }
    dm.createServiceManager<TestMsgGlazeSerializer, ISerializer<TestMsg>>();
#ifdef URING_EXAMPLE
    dm.createServiceManager<HOSTIMPL, IHostService>(Properties{{"Address", Ichor::v1::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::v1::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
    dm.createServiceManager<ClientFactory<CONNIMPL<IClientConnectionService>, IClientConnectionService>, IClientFactory<IClientConnectionService>>();
#endif
    dm.createServiceManager<WSHOSTIMPL, IHostService>(Properties{{"Address", Ichor::v1::make_any<std::string>(address)}, {"Port", Ichor::v1::make_any<uint16_t>(static_cast<uint16_t>(8001))}}, priorityToEnsureHostStartingFirst);
    dm.createServiceManager<ClientFactory<WSCONNIMPL<IConnectionService>>, IClientFactory<IConnectionService>>();
    dm.createServiceManager<UsingWsService>(Properties{{"Address", Ichor::v1::make_any<std::string>(address)}, {"Port", Ichor::v1::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
    queue->start(CaptureSigInt);
    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}
