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
#include <ichor/services/logging/CoutLogger.h>
#include <fmt/format.h>
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
                            "MIIJQQIBADANBgkqhkiG9w0BAQEFAASCCSswggknAgEAAoICAQCoibPiYhYsJrX9\n"
                            "fU8Iszx3Pk7pHbZ+Fi1lADHvZyw1fUJxZPxhWxIDwR7d7o7V15PHw/Xks6FrhwJR\n"
                            "V9DrC1mIixeBqeyo/K/b7D/QA5LYXDi7GiSGfaXUEevOWSP0XTdFpo5S0LIZ2QXX\n"
                            "WDULXcYHP+ABdIqKVLc9t98Y/ksaft5cU4WJeieMeJuT005Hw5Nzj6t99lj7aC/H\n"
                            "rdBqVJVwIxypRbgH7V0Bcz7ZvRyI03yF5J5mU32n9RUsgbiD9x4njCHOAFzhIXgR\n"
                            "+Al9I2Hy5sQ8icZJOjqd5CFa+DAIWQuyKVljOXIOl0xvHW+y2SGE1knoGNcw5alQ\n"
                            "81WN2ZhFjKwSBldruP9PjUH4ixEuFB6DjKKkkU3U3QbquNYRgEGPPRWvjd5Qe++R\n"
                            "vQmtrKkPKilNpxCZrsf9fcdUvZSbh5y63u3t6UTCc63LkVS94G4LiU8uyURHz51+\n"
                            "NcNCD6I9+m1lLmUezt5/bv3oTb0+QTaWrf7Pak5AS6ny76BCgernV8ZyIMK/2uel\n"
                            "5ezrJR0+9l9of2O6TWyF1Z5FRy6yRVD8xPw0EypN6rkhI73LAIQ3smqXzE+Iqf1G\n"
                            "+wq0ZzNch48zln9c78+Pn8300Gar20DQqw07plxNG0zlZrHW5PBXkk2Ru0/mtxOU\n"
                            "0QAUaDAzjbT2jDcdaw5BVFaE/NPLlwIDAQABAoICACbajAZKD4uBJx53t3MtguiI\n"
                            "jD+QsoQRz8hDib+DvPzd0iX7HnXBPX9xE5EdUH968d4xTmw5fyKfXsjq4kZ1eOAS\n"
                            "FMq3JlB05IPiTHnDgSRw6kE0DyyUJdkkgoSxJylMHhRoB9KOuyhbUE3rT1s99Iuy\n"
                            "TY14VQH78rx/Oab05NKIYfHN7XCaoShwm4kyQw8nRsRy4BmrY6sdwzoY133T3xmZ\n"
                            "dp+Na5YkQfc2HsyqnLvsaX4ijOPRJpxEU2vNwgBmRWTfmOF5UvAxu5EE6gcX05pg\n"
                            "WegM6RHzoxTIRWpH4ibihGA/yRQnZ6R2e8/37MrB5MjPb/1aUK28nFKFrnkDq2cp\n"
                            "SZY9risLePvAs6+6ivB45q5220C+4Q8ouo1iDa7BONOMwiWQ3bifYVLZo86axy0R\n"
                            "Y2hksXXiqP7U1xM/7J1YTkNwSSQAaNzJBc/PP8BYawUrBOA2DL92iOJHpmLYgr0V\n"
                            "JZ7kS3d3/7Ne7FZBmSHIk8+3MCRCnRXuUyzdQL9vC08EGHyUGkf7iNO9Hgmt0tDD\n"
                            "HKiBYQM/J2GJUEJgreE2Zd8WJAsN4YSNEmJTxWST1jVrCH8LFyxOdpIPax/1BQRN\n"
                            "U6R44QoQAWzToKlWB6d5VdHcD3Cllx6LgVruS4eON0JaWpgvog/34HtRPQgXmDwK\n"
                            "/j2u66R7WQad9y2nRG8FAoIBAQDDWtHxg5+rCim0xtyufMWYLYlpbCHkOsnm1DQz\n"
                            "clTneGd0DP3dfXU8DxK5QnPq7+RP3Rth2cDmoYffB4qCXKIHvBFvwnpMGWlj19M7\n"
                            "CdP/J9+k5AMBRfizQkyw0peOHDIi7vQkv+7rfx3heuhB5BkPWFEu7aqsnAlwHdmt\n"
                            "4zt+obuTk41vNdzUlicAf548g5KdoEE4BF7nwO9eGL/PnY/aXug8HpRkidCxdPbC\n"
                            "V6gMiYIVf0VdkJjQfNl4LkIkU2PwNfUxrRGElJqvWZclSLOooRFqPF1I8lVbJ2+G\n"
                            "6KXxMJh4f8Yt+YnMy5pc/vvf9bfKXv05s1kcyBaY8A27rAT1AoIBAQDc27M6nT3h\n"
                            "9ArrFigzl+PQg+SrMbR/eynnvb71GdjCcQQbqJcRz4BrPwx/p2aDXT+Mgfa1yxkQ\n"
                            "01TyOdQPFOD5anJ5qCNu/7sAL7EjDXkBAz/PNDElMfRWHwuFhZioNswOwPAjB6ln\n"
                            "RWZFBXVRSXyY8AhLVJ/Rkv54GbsxXCnLTGOqp9AsODULtbT7k6Gl/jwrYVT59onl\n"
                            "vXgTB5Bp+1ve2bB/Fja5IYuKOe7aayM6jh5qp+8a05XFvza4/GWEThOt025aj8Xm\n"
                            "CGyBI/VQnmfgY6j072PQ7zBBzecibYAv5S7uEnGIXleHKLRnCT+yQLVEeHPB967f\n"
                            "IzhlyQCUNJbbAoIBADl5McT9NW9rqQN/chN/r95qnz+9yWbNCPN5QPZVz7bdzInu\n"
                            "/I06SNBnmtmYPGRFOYVphpHOL5tqsH+kR1K9EAp4gSTBMZvL6h6us31uEcnCoGBx\n"
                            "mJS6UkXi3o8zFdWAZBu0820QbqkoF9FriINCKUFDeXb2Az2PFpO+pHktHibOTFJJ\n"
                            "mdW7IgIFPuo9oX8qMmtfZ3CkkItcTs//NgdN0rGrNGXjlULS7OwYfjSE93Lgzb2n\n"
                            "WPADB4JnCM/7xAfN5NwZ884unbXExpGKKRaWAg09fdMkeUlpykTsIDqvnYqNAn6J\n"
                            "EPQbszfmOr7bYZztPKo0MgnUwwXdtBBAp7msDTUCggEAMWhAtxsYDeUvUBn70lbn\n"
                            "Uu0+iYGsFFy0KjCLeH9Yu17XAWz8prJZ0yQeoqwLRdXlcoRccZzGtbnhJfMD2n/c\n"
                            "OE+03jxb+OfiqI8CvIm2CSD19F+DowOC2oVFaZBSb0ca7S4CSoPbRMwEreojrZSX\n"
                            "+AqygE+lFRgiLzHqa4dnniAvp6y06D+GtmDm5OTI4751LWsvvF+Hx8pUA0XM67ic\n"
                            "e8UNM9D/WvkDI2AEa/5Nujqy2T9KQSEWP0+gaU+lUiy5QEitRjsllWlLBLTLm0Zk\n"
                            "jJEe6fRnywTHMeFjFJOUeqJ1ljKwDUa1o/naKbaq9jB9nJypoLnM+AQECNkndQAy\n"
                            "wwKCAQBgSBXkPTWVVxu85Hi5dLGb4co4UmZmPLJEoeun3OzNE5iqUUt7ZwSp073S\n"
                            "8OJNQNS8Lw6uHxgyDmfPRpTG13V/zFEPFeEeNY7PKsy83gC6/zfse4DqErlAZ2b7\n"
                            "F8ZNoKdB3DoCgfoNtlQdfac7XQCfv86EG1+N2n73H+hQS+0GQTIBZNprQXVH+v/N\n"
                            "YSp0DqX2NoebaiJPdlArVD34eq4mpIAjt7KBA4cPWylXgUwngIsK06cuZMBFofII\n"
                            "tDOU/DNeusyw1dUlc8rvATyRZ1Q6UeZcGmz59n/cAn0VZwLBgpksWb2nCsIdg2A5\n"
                            "qK0vv2FNohzE6aFZJG1P7u9YFjC8\n"
                            "-----END PRIVATE KEY-----\n";

                // notAfter=Jul 19 13:34:25 2298 GMT
                std::string cert = "-----BEGIN CERTIFICATE-----\n"
                                   "MIIFVTCCAz2gAwIBAgIUI1RiT57L/Xb2uaSwqnMKxVv5rDcwDQYJKoZIhvcNAQEL\n"
                                   "BQAwOTELMAkGA1UEBhMCTkwxEzARBgNVBAgMClNvbWUtU3RhdGUxFTATBgNVBAoM\n"
                                   "DFZvbHRTb2Z0d2FyZTAgFw0yNDEwMDQxMzM0MjVaGA8yMjk4MDcxOTEzMzQyNVow\n"
                                   "OTELMAkGA1UEBhMCTkwxEzARBgNVBAgMClNvbWUtU3RhdGUxFTATBgNVBAoMDFZv\n"
                                   "bHRTb2Z0d2FyZTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAKiJs+Ji\n"
                                   "Fiwmtf19TwizPHc+Tukdtn4WLWUAMe9nLDV9QnFk/GFbEgPBHt3ujtXXk8fD9eSz\n"
                                   "oWuHAlFX0OsLWYiLF4Gp7Kj8r9vsP9ADkthcOLsaJIZ9pdQR685ZI/RdN0WmjlLQ\n"
                                   "shnZBddYNQtdxgc/4AF0iopUtz233xj+Sxp+3lxThYl6J4x4m5PTTkfDk3OPq332\n"
                                   "WPtoL8et0GpUlXAjHKlFuAftXQFzPtm9HIjTfIXknmZTfaf1FSyBuIP3HieMIc4A\n"
                                   "XOEheBH4CX0jYfLmxDyJxkk6Op3kIVr4MAhZC7IpWWM5cg6XTG8db7LZIYTWSegY\n"
                                   "1zDlqVDzVY3ZmEWMrBIGV2u4/0+NQfiLES4UHoOMoqSRTdTdBuq41hGAQY89Fa+N\n"
                                   "3lB775G9Ca2sqQ8qKU2nEJmux/19x1S9lJuHnLre7e3pRMJzrcuRVL3gbguJTy7J\n"
                                   "REfPnX41w0IPoj36bWUuZR7O3n9u/ehNvT5BNpat/s9qTkBLqfLvoEKB6udXxnIg\n"
                                   "wr/a56Xl7OslHT72X2h/Y7pNbIXVnkVHLrJFUPzE/DQTKk3quSEjvcsAhDeyapfM\n"
                                   "T4ip/Ub7CrRnM1yHjzOWf1zvz4+fzfTQZqvbQNCrDTumXE0bTOVmsdbk8FeSTZG7\n"
                                   "T+a3E5TRABRoMDONtPaMNx1rDkFUVoT808uXAgMBAAGjUzBRMB0GA1UdDgQWBBRS\n"
                                   "MnXR+jz75pQgMym6zXtn/QpPXDAfBgNVHSMEGDAWgBRSMnXR+jz75pQgMym6zXtn\n"
                                   "/QpPXDAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4ICAQAY7PmtMcWS\n"
                                   "XVPDESOz9xaXPKfdyv4YW1wMe+6+PeOfcfw16c2zezV2BEx1PmQYNT/AcfP7nq65\n"
                                   "AnuKoEyv058opl2W1rZMDNsKuKWkhs5vzdXD1C6d6jCy1QSxKESFRY79N5JHthUS\n"
                                   "qBnW1XWq9/W7HrVwx3SM8DAo03DsykqYKKQ7v8KAyHtRRV9Csl2Td+QVtPSvSqTS\n"
                                   "Y7ak+ltA+XbkcGAUq8HJhU+wZoaqTBl5YcJ6x3BC7rsBlnFo8Wmk7bvRG4oKFMPH\n"
                                   "cEGUTodcqtaeJ2ob16iAouAbLMM5shY8rP/yyVo6aoiHjb8JQ/Xn1GtulvNs+hQm\n"
                                   "xpt51RafCUlfvvcOL0zDjgn82BPVulpBZZdRaoq7N7zZqu3Xa/EyrKv1wfzEN/mi\n"
                                   "VgyrbcbHtdUf4jhthhW91iAKkKn2YSwyUqwq3Fhx5SluFgieUAL3EQ+hffTUM2tI\n"
                                   "4usT9LkCKuR8LNZs4i7VsTFyglHXwXg63rtgsv2RVMyLWuHAySTuG+LO9F8/alC+\n"
                                   "rLoo/tz8ji1x0FWvnSREinerzqYtcNRIb1RXb73uCotHXMsyFZvBf/DSdoTXvDts\n"
                                   "WzExDxel3FNj6YiXhgHuu0/Vegt4bmqLQZas7Zc3O0GjxWYe+gFqpSZShYrTZ5ig\n"
                                   "G8ZTcl8MLgiy4+OYgYkEipe+7QYiCnvR/g==\n"
                                   "-----END CERTIFICATE-----\n";

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
