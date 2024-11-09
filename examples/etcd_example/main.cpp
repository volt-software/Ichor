#include "UsingEtcdService.h"
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/NullFrameworkLogger.h>
#include <ichor/services/etcd/EtcdV2Service.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/services/network/boost/HttpConnectionService.h>
#include <ichor/services/network/boost/AsioContextService.h>
#include <ichor/services/network/ClientFactory.h>
#include <ichor/ichor-mimalloc.h>

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
    try {
        std::locale::global(std::locale("en_US.UTF-8"));
    } catch(std::runtime_error const &e) {
        fmt::println("Couldn't set locale to en_US.UTF-8: {}", e.what());
    }

    auto start = std::chrono::steady_clock::now();
    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->createManager();
#ifdef ICHOR_USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
    dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
    dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_INFO)}});
    dm.createServiceManager<Etcd::v2::EtcdService, Etcd::v2::IEtcd>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(2379))}, {"TimeoutMs", Ichor::make_any<uint64_t>(1'000ul)}});
    dm.createServiceManager<UsingEtcdV2Service>();
    dm.createServiceManager<Boost::AsioContextService, Boost::IAsioContextService>();
    dm.createServiceManager<ClientFactory<Boost::HttpConnectionService, IHttpConnectionService>, IClientFactory>();
    dm.createServiceManager<TimerFactoryFactory>();
    queue->start(CaptureSigInt);
    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}
