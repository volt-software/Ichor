#ifdef ICHOR_USE_ETCD

#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <ichor/services/logging/CoutLogger.h>
#include <ichor/services/etcd/EtcdService.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/services/network/http/HttpConnectionService.h>
#include <ichor/services/network/AsioContextService.h>
#include <ichor/services/network/ClientFactory.h>
#include "TestServices/EtcdUsingService.h"
#include "Common.h"

using namespace Ichor;

TEST_CASE("EtcdTests") {
    SECTION("Set/get") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, 10);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            dm.createServiceManager<AsioContextService, IAsioContextService>();
            dm.createServiceManager<ClientFactory<HttpConnectionService, IHttpConnectionService>>();
            dm.createServiceManager<EtcdService, IEtcd>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(2379))}, {"TimeoutMs", Ichor::make_any<uint64_t>(1'000ul)}});
            dm.createServiceManager<EtcdUsingService>();
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

