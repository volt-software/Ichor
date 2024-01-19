#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/network/boost/HttpHostService.h>
#include <ichor/services/network/boost/HttpConnectionService.h>
#include <ichor/services/network/boost/AsioContextService.h>
#include <ichor/services/network/ClientFactory.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <ichor/services/logging/CoutLogger.h>
#include "Common.h"
#include "TestServices/HttpThreadService.h"
#include "../examples/common/TestMsgGlazeSerializer.h"
#include "serialization/RegexJsonMsgSerializer.h"

using namespace Ichor;

std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;
std::thread::id testThreadId;
std::thread::id dmThreadId;
std::atomic<bool> evtGate;

TEST_CASE("HttpTests") {
    SECTION("Http events on same thread") {
        testThreadId = std::this_thread::get_id();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
        auto queue = std::make_unique<PriorityQueue>(true);
        auto &dm = queue->createManager();
        evtGate = false;

        std::thread t([&]() {
            dmThreadId = std::this_thread::get_id();

            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, 10);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            dm.createServiceManager<TestMsgGlazeSerializer, ISerializer<TestMsg>>();
            dm.createServiceManager<RegexJsonMsgSerializer, ISerializer<RegexJsonMsg>>();
            dm.createServiceManager<AsioContextService, IAsioContextService>();
            dm.createServiceManager<HttpHostService, IHttpHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
            dm.createServiceManager<ClientFactory<HttpConnectionService, IHttpConnectionService>, IClientFactory>();
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
            fmt::print("1 _evt set\n");
            _evt->set();
        });

        t.join();
    }

    SECTION("Https events on same thread") {
        testThreadId = std::this_thread::get_id();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
        auto queue = std::make_unique<PriorityQueue>(true);
        auto &dm = queue->createManager();
        evtGate = false;

        std::thread t([&]() {
            dmThreadId = std::this_thread::get_id();

            std::string key = "-----BEGIN PRIVATE KEY-----\n"
                              "MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQDIisWpB9FSc/OI\n"
                              "YsxznQJgioL2aKlqf0fKzrP6IVSq1ApxlMF+hVpz340Yv5rVYizfb/zx46pTbPPJ\n"
                              "+VNc63blCPAIiL7h3NdYsMaAe+dTfdmQd2/m1kiBQFraceOw5TB7+Y9lFXfUCUFF\n"
                              "elbLf7JTW80CnNOP4xPmXCusMD8m4LL8z7e7BqKr4VFrrAFXB0EBqpmHA8HXbKAi\n"
                              "1QbWD3G1lkTQcCrq8c7pw5oME0RNjfrevlGHATJ2a6zCAeUsIfsDGFpNHFIYqGGo\n"
                              "swxe5IF+yB6fWFWMjtbbRv93pZfViTFcOUWQuhLFsSN41bt9/XimlxM+ZtX5JDE0\n"
                              "yv+K0iUBAgMBAAECggEAB2lJgMOvMrLiTyoHkEY/Lj4wNNcNW8g0aQRWlmng7SdE\n"
                              "84mh1QEspJegaUe7eyNTsTY8TNwzET43jEFQmWCCVliMNmSHWWWF99sgmuL5W5aN\n"
                              "Ec+4LPnCWDR+pxAKcCEoN4yzhfLTKNzmsqCg0Ih5mKcN3ojZMLodpCfH3WczDkay\n"
                              "1KrJvIKIovIV8Yv8NJ4K9hDx7a+xGpHXqh5NLey9K6XNpDqbWwN97FygLLEoedhx\n"
                              "XDjPzBiGJuwIYQI/V2gYVXn3nPcglDFqHpZA8peX3+DwXojqDwzbNhf2CANpMgaD\n"
                              "5OQkGymb+FumKPh1UMEL+IfqtXWwwg2Ix7DtI6i/IQKBgQDasFuUC43F0NM61TFQ\n"
                              "hWuH4mRMp8B4YkhO9M+0/mitHI76cnU63EnJpMYvnp3CLRpVzs/sidEcbfC9gOma\n"
                              "Ui+xxtKers+RQDI4lrwgqyYIzNNpGO7ItlDYs3guTvX5Mvc5x/zIIHhcjhTudzHK\n"
                              "Utgho8PIXvrrF3fSHdL0QJb1tQKBgQDqwdGImGbF3lxsB8t4T/8fozIBwo2dWp4O\n"
                              "YgcVyDpq7Nl+J9Yy5fqTw/sIn6Fa6ZpqAzq0zPSoYJzU7l9+5CcpkkdPEulyER3D\n"
                              "maQaVMUv0cpnXUQSBRNDZJDdbC4eKocxxotyTuLjCVrUwYWuqYWo3aGx42VXWsWm\n"
                              "n4+9AwDBnQKBgBStumscUJKU9XRJtnkLtKhLsvpAnoWDnZzBr2ZI7DL6UVbDPeyL\n"
                              "6fpEN21HTVmQFD5q6ORP/9L1Xl888lniTZo816ujkgMFE/qf3jgkltscKx1z+xhF\n"
                              "jQ2AouuWEdI3jIMNMwzlbRwrXzVRVgbwoHlF1/x5ZraWKIFYyprIBL5FAoGAfXcM\n"
                              "12YsN0A6QPqBglGu1mfQCCTErv6JTsKRatDSd+cR7ly4HAfRvjuV5Ov7vqzu/A2x\n"
                              "yINplrvb1el4XEbvr0YgmmBPJ8mCENICZJg9sur6s/eis8bGntQWoGB63WB5VN76\n"
                              "FCOZGyIay26KVekAKFobWwlfViqLTBwnJCuAsfkCgYAgrbUKCf+BGgz4epv3T3Ps\n"
                              "kJD2XX1eeQr/bw1IMqY7ZLKCjg7oxM1jjp2xyujY3ZNViaJsnj7ngCIbaOSJNa3t\n"
                              "YS8GDLTL4efNTmPEVzgxhnJdSeYVgMLDwu66M9EYhkw+0Y1PQsNS1+6SO89ySRzP\n"
                              "pFbool/8ZDecmB4ZSa03aw==\n"
                              "-----END PRIVATE KEY-----";

            std::string cert = "-----BEGIN CERTIFICATE-----\n"
                               "MIIDdzCCAl+gAwIBAgIUQEtUTmX2HbHwOpDPgFkYx66ei/swDQYJKoZIhvcNAQEL\n"
                               "BQAwSzELMAkGA1UEBhMCTkwxEjAQBgNVBAcMCUFtc3RlcmRhbTEOMAwGA1UECgwF\n"
                               "SWNob3IxGDAWBgNVBAMMD3d3dy5leGFtcGxlLmNvbTAeFw0yMzEwMDIwODI1NTVa\n"
                               "Fw0yNDEwMDEwODI1NTVaMEsxCzAJBgNVBAYTAk5MMRIwEAYDVQQHDAlBbXN0ZXJk\n"
                               "YW0xDjAMBgNVBAoMBUljaG9yMRgwFgYDVQQDDA93d3cuZXhhbXBsZS5jb20wggEi\n"
                               "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDIisWpB9FSc/OIYsxznQJgioL2\n"
                               "aKlqf0fKzrP6IVSq1ApxlMF+hVpz340Yv5rVYizfb/zx46pTbPPJ+VNc63blCPAI\n"
                               "iL7h3NdYsMaAe+dTfdmQd2/m1kiBQFraceOw5TB7+Y9lFXfUCUFFelbLf7JTW80C\n"
                               "nNOP4xPmXCusMD8m4LL8z7e7BqKr4VFrrAFXB0EBqpmHA8HXbKAi1QbWD3G1lkTQ\n"
                               "cCrq8c7pw5oME0RNjfrevlGHATJ2a6zCAeUsIfsDGFpNHFIYqGGoswxe5IF+yB6f\n"
                               "WFWMjtbbRv93pZfViTFcOUWQuhLFsSN41bt9/XimlxM+ZtX5JDE0yv+K0iUBAgMB\n"
                               "AAGjUzBRMB0GA1UdDgQWBBRgTmWQZDeiFMXtL8tyQSdD/HRYPjAfBgNVHSMEGDAW\n"
                               "gBRgTmWQZDeiFMXtL8tyQSdD/HRYPjAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3\n"
                               "DQEBCwUAA4IBAQC1QKNwlqrvE5a7M5v151tA/JvIKNBfesIRCg/IG6tB3teSJjAq\n"
                               "PRsSajMcyvDtlDMkWL093B8Epsc8yt/vN1GgsCfQsMeIWRGvRYM6Qo2WoABaLTuC\n"
                               "TxH89YrXa9uwUnbesGwKSkpxzj8g6YuzUpLnAAV70LefBn8ZgBJktFVW/ANWwQqD\n"
                               "KiglduSDGX3NiLByn7nGKejg2dPw6kWpOTVtLsWtgG/5T4vw+INQzPJqy+pUvBeQ\n"
                               "OrV7cQyq12DJJPxboIRh1KqF8C25NAOnXVtCav985RixHCMaNaXyOXBJeEEASnXg\n"
                               "pdDrObCgMze03IaHQ1pj3LeMu0OvEMbsRrYW\n"
                               "-----END CERTIFICATE-----";

            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, 10);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            dm.createServiceManager<TestMsgGlazeSerializer, ISerializer<TestMsg>>();
            dm.createServiceManager<RegexJsonMsgSerializer, ISerializer<RegexJsonMsg>>();
            dm.createServiceManager<AsioContextService, IAsioContextService>();
            dm.createServiceManager<HttpHostService, IHttpHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}, {"SslKey", Ichor::make_any<std::string>(key)}, {"SslCert", Ichor::make_any<std::string>(cert)}});
            dm.createServiceManager<ClientFactory<HttpConnectionService, IHttpConnectionService>, IClientFactory>();
            dm.createServiceManager<HttpThreadService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}, {"ConnectOverSsl", Ichor::make_any<bool>(true)}, {"RootCA", Ichor::make_any<std::string>(cert)}});

            queue->start(CaptureSigInt);
        });

        while(!evtGate) {
            std::this_thread::sleep_for(500us);
        }

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(Ichor::Detail::_local_dm == &dm);
            REQUIRE(testThreadId != std::this_thread::get_id());
            REQUIRE(dmThreadId == std::this_thread::get_id());
            fmt::print("2 _evt set\n");
            _evt->set();
        });

        t.join();
    }

    SECTION("Http events on 4 threads") {
        testThreadId = std::this_thread::get_id();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
        auto queue = std::make_unique<PriorityQueue>(true);
        auto &dm = queue->createManager();
        evtGate = false;

        std::thread t([&]() {
            dmThreadId = std::this_thread::get_id();

            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, 10);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            dm.createServiceManager<TestMsgGlazeSerializer, ISerializer<TestMsg>>();
            dm.createServiceManager<RegexJsonMsgSerializer, ISerializer<RegexJsonMsg>>();
            dm.createServiceManager<AsioContextService, IAsioContextService>(Properties{{"Threads", Ichor::make_any<uint64_t>(4ul)}});
            dm.createServiceManager<HttpHostService, IHttpHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
            dm.createServiceManager<ClientFactory<HttpConnectionService, IHttpConnectionService>, IClientFactory>();
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
            fmt::print("3 _evt set\n");
            _evt->set();
        });

        t.join();
    }
}

#else

#include "Common.h"

TEST_CASE("HttpTests") {
    SECTION("Empty Test so that Catch2 exits with 0") {
        REQUIRE(true);
    }
}

#endif
