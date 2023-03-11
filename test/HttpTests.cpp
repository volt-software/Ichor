#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/network/http/HttpHostService.h>
#include <ichor/services/network/http/HttpConnectionService.h>
#include <ichor/services/network/AsioContextService.h>
#include <ichor/services/network/ClientFactory.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <ichor/services/logging/CoutLogger.h>
#include "Common.h"
#include "TestServices/HttpThreadService.h"
#include "../examples/common/TestMsgRapidJsonSerializer.h"

using namespace Ichor;

std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;
std::thread::id testThreadId;
std::thread::id dmThreadId;
std::atomic<bool> evtGate;

TEST_CASE("HttpTests") {
    SECTION("Http events on same thread") {
        testThreadId = std::this_thread::get_id();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
        auto queue = std::make_unique<MultimapQueue>(true);
        auto &dm = queue->createManager();
        evtGate = false;

        std::thread t([&]() {
            dmThreadId = std::this_thread::get_id();

            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, 10);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            dm.createServiceManager<TestMsgRapidJsonSerializer, ISerializer<TestMsg>>();
            dm.createServiceManager<AsioContextService, IAsioContextService>();
            dm.createServiceManager<HttpHostService, IHttpService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
            dm.createServiceManager<ClientFactory<HttpConnectionService, IHttpConnectionService>>();
            dm.createServiceManager<HttpThreadService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}});

            queue->start(CaptureSigInt);
        });

        while(!evtGate) {
            std::this_thread::sleep_for(500us);
        }

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(Ichor::Detail::_local_dm == &dm);
            REQUIRE(testThreadId != std::this_thread::get_id());
            REQUIRE(dmThreadId == std::this_thread::get_id());
            _evt->set();
        });

        t.join();
    }

    SECTION("Http events on 4 threads") {
        testThreadId = std::this_thread::get_id();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
        auto queue = std::make_unique<MultimapQueue>(true);
        auto &dm = queue->createManager();
        evtGate = false;

        std::thread t([&]() {
            dmThreadId = std::this_thread::get_id();

            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, 10);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            dm.createServiceManager<TestMsgRapidJsonSerializer, ISerializer<TestMsg>>();
            dm.createServiceManager<AsioContextService, IAsioContextService>(Properties{{"Threads", Ichor::make_any<uint64_t>(4ul)}});
            dm.createServiceManager<HttpHostService, IHttpService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
            dm.createServiceManager<ClientFactory<HttpConnectionService, IHttpConnectionService>>();
            dm.createServiceManager<HttpThreadService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}});

            queue->start(CaptureSigInt);
        });

        while(!evtGate) {
            std::this_thread::sleep_for(500us);
        }

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(Ichor::Detail::_local_dm == &dm);
            REQUIRE(testThreadId != std::this_thread::get_id());
            REQUIRE(dmThreadId == std::this_thread::get_id());
            _evt->set();
        });

        t.join();
    }
}

#endif
