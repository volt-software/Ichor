#include "UsingTcpService.h"
#include "../common/TestMsgGlazeSerializer.h"
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

#ifdef URING_EXAMPLE
#include <ichor/services/network/tcp/IOUringTcpConnectionService.h>
#include <ichor/services/network/tcp/IOUringTcpHostService.h>
#include <ichor/event_queues/IOUringQueue.h>

#define QIMPL IOUringQueue
#define CONNIMPL IOUringTcpConnectionService
#define HOSTIMPL IOUringTcpHostService
#else
#include <ichor/services/network/tcp/TcpConnectionService.h>
#include <ichor/services/network/tcp/TcpHostService.h>
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/services/timer/TimerFactoryFactory.h>

#define QIMPL PriorityQueue
#define CONNIMPL TcpConnectionService
#define HOSTIMPL TcpHostService
#endif

#include <chrono>
#include <iostream>

using namespace std::string_literals;

int main(int argc, char *argv[]) {
    try {
        std::locale::global(std::locale("en_US.UTF-8"));
    } catch(std::runtime_error const &e) {
        fmt::println("Couldn't set locale to en_US.UTF-8: {}", e.what());
    }

    auto start = std::chrono::steady_clock::now();
    auto queue = std::make_unique<QIMPL>(500);
#ifdef URING_EXAMPLE
    if(!queue->createEventLoop()) {
        fmt::println("Couldn't create io_uring event loop");
        return -1;
    }
#endif
    auto &dm = queue->createManager();

    uint64_t priorityToEnsureHostStartingFirst = 51;

#ifdef ICHOR_USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>(Properties{}, priorityToEnsureHostStartingFirst);
#endif
    dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>(Properties{{"LogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
    dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
    dm.createServiceManager<TestMsgGlazeSerializer, ISerializer<TestMsg>>();
    dm.createServiceManager<HOSTIMPL, IHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}}, priorityToEnsureHostStartingFirst);
    dm.createServiceManager<ClientFactory<CONNIMPL<IClientConnectionService>>, IClientFactory>();
#ifndef URING_EXAMPLE
    dm.createServiceManager<TimerFactoryFactory>(Properties{}, priorityToEnsureHostStartingFirst);
#endif
    dm.createServiceManager<UsingTcpService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
    queue->start(CaptureSigInt);
    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}
