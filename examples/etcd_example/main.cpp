#include "UsingEtcdService.h"
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/NullFrameworkLogger.h>
#include <ichor/services/etcd/EtcdV2Service.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/services/network/ClientFactory.h>
#include <ichor/ichor-mimalloc.h>

#if defined(URING_EXAMPLE)
#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/services/network/http/HttpConnectionService.h>
#include <ichor/services/network/tcp/IOUringTcpConnectionService.h>
#include <ichor/services/timer/IOUringTimerFactoryFactory.h>

#define QIMPL IOUringQueue
#define CONNIMPL IOUringTcpConnectionService
#define HTTPCONNIMPL HttpConnectionService
#define TFFIMPL IOUringTimerFactoryFactory
#else
#include <ichor/services/network/boost/HttpConnectionService.h>
#include <ichor/event_queues/BoostAsioQueue.h>
#include <ichor/services/timer/TimerFactoryFactory.h>

#define QIMPL BoostAsioQueue
#define HTTPCONNIMPL Boost::v1::HttpConnectionService
#define TFFIMPL TimerFactoryFactory
#endif

// Some compile time logic to instantiate a regular cout logger or to use the spdlog logger, if Ichor has been compiled with it.
#ifdef ICHOR_USE_SPDLOG
#include <ichor/services/logging/SpdlogLogger.h>
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/services/logging/CoutLogger.h>
#define LOGGER_TYPE CoutLogger
#endif

#include <chrono>
#include <iostream>

using namespace std::string_literals;

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

    auto start = std::chrono::steady_clock::now();
    auto queue = std::make_unique<QIMPL>();
    auto &dm = queue->createManager();
#ifdef URING_EXAMPLE
    if(!queue->createEventLoop()) {
        fmt::println("Couldn't create io_uring event loop");
        return -1;
    }
#endif
#ifdef ICHOR_USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
    dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
    dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_INFO)}});
    dm.createServiceManager<Etcdv2::v1::EtcdService, Etcdv2::v1::IEtcd>(Properties{{"Address", Ichor::v1::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::v1::make_any<uint16_t>(static_cast<uint16_t>(2379))}, {"TimeoutMs", Ichor::v1::make_any<uint64_t>(1'000ul)}});
    dm.createServiceManager<UsingEtcdV2Service>();
#ifdef URING_EXAMPLE
    dm.createServiceManager<ClientFactory<CONNIMPL<IClientConnectionService>, IClientConnectionService>, IClientFactory<IClientConnectionService>>();
#endif
    dm.createServiceManager<ClientFactory<HTTPCONNIMPL, IHttpConnectionService>, IClientFactory<IHttpConnectionService>>();
    dm.createServiceManager<TFFIMPL>();
    queue->start(CaptureSigInt);
    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}
