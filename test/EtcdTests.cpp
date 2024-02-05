#ifdef ICHOR_USE_ETCD

#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/etcd/EtcdV2Service.h>
#include <ichor/services/etcd/EtcdV3Service.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/services/network/boost/HttpConnectionService.h>
#include <ichor/services/network/boost/AsioContextService.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <ichor/services/network/ClientFactory.h>
#include "TestServices/Etcdv2UsingService.h"
#include "TestServices/Etcdv3UsingService.h"
#include "Common.h"

#ifdef ICHOR_USE_SPDLOG
#include <ichor/services/logging/SpdlogLogger.h>
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/services/logging/CoutLogger.h>
#define LOGGER_TYPE CoutLogger
#endif

using namespace Ichor;

TEST_CASE("EtcdTests") {
    SECTION("v2") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, 10);
#ifdef ICHOR_USE_SPDLOG
            dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
            dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            dm.createServiceManager<AsioContextService, IAsioContextService>();
            dm.createServiceManager<ClientFactory<HttpConnectionService, IHttpConnectionService>, IClientFactory>();
            dm.createServiceManager<Etcd::v2::EtcdService, Etcd::v2::IEtcd>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(2379))}, {"TimeoutMs", Ichor::make_any<uint64_t>(1'000ul)}, {"Debug", Ichor::make_any<bool>(true)}});
            dm.createServiceManager<Etcdv2UsingService>(Properties{{"LogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            dm.createServiceManager<TimerFactoryFactory>();

            queue->start(CaptureSigInt);
        });

        t.join();
    }

    SECTION("v3") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, 10);
#ifdef ICHOR_USE_SPDLOG
            dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
            dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            dm.createServiceManager<AsioContextService, IAsioContextService>();
            dm.createServiceManager<ClientFactory<HttpConnectionService, IHttpConnectionService>, IClientFactory>();
            dm.createServiceManager<Etcd::v3::EtcdService, Etcd::v3::IEtcd>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(2379))}, {"TimeoutMs", Ichor::make_any<uint64_t>(1'000ul)}, {"Debug", Ichor::make_any<bool>(true)}});
            dm.createServiceManager<Etcdv3UsingService>(Properties{{"LogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            dm.createServiceManager<TimerFactoryFactory>();

            queue->start(CaptureSigInt);
        });

        t.join();
    }
}

#else

#include "Common.h"

TEST_CASE("EtcdTests") {
    SECTION("Empty Test so that Catch2 exits with 0") {
        REQUIRE(true);
    }
}

#endif

