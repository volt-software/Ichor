#include "UsingEtcdService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/etcd/EtcdV2Service.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/services/network/http/HttpConnectionService.h>
#include <ichor/services/network/AsioContextService.h>
#include <ichor/services/network/ClientFactory.h>

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
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::steady_clock::now();
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
#ifdef ICHOR_USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
    dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_INFO)}});
    dm.createServiceManager<EtcdV2Service, IEtcd>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(2379))}, {"TimeoutMs", Ichor::make_any<uint64_t>(1'000ul)}});
    dm.createServiceManager<UsingEtcdV2Service>();
    dm.createServiceManager<AsioContextService, IAsioContextService>();
    dm.createServiceManager<ClientFactory<HttpConnectionService, IHttpConnectionService>, IClientFactory>();
    dm.createServiceManager<TimerFactoryFactory>();
    queue->start(CaptureSigInt);
    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}
