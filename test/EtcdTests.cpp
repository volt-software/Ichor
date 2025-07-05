#include "Common.h"
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/etcd/EtcdV2Service.h>
#include <ichor/services/etcd/EtcdV3Service.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/services/network/ClientFactory.h>
#include "TestServices/Etcdv2UsingService.h"
#include "TestServices/Etcdv3UsingService.h"

#ifdef ICHOR_USE_SPDLOG
#include <ichor/services/logging/SpdlogLogger.h>
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/services/logging/CoutLogger.h>
#define LOGGER_TYPE CoutLogger
#endif

#ifdef TEST_BOOST
#include <ichor/services/network/boost/HttpConnectionService.h>
#include <ichor/event_queues/BoostAsioQueue.h>


#define QIMPL BoostAsioQueue
#define HTTPCONNIMPL Boost::v1::HttpConnectionService
#elif defined(TEST_URING)
#include <ichor/services/network/tcp/IOUringTcpConnectionService.h>
#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/services/network/http/HttpConnectionService.h>
#include <catch2/generators/catch_generators.hpp>
#include <ichor/stl/LinuxUtils.h>

using namespace std::string_literals;

#define QIMPL IOUringQueue
#define CONNIMPL IOUringTcpConnectionService
#define HTTPCONNIMPL HttpConnectionService
#else
#error "no uring/boost"
#endif

using namespace Ichor;
#if defined(TEST_URING)
tl::optional<v1::Version> emulateKernelVersion;

TEST_CASE("EtcdTests_uring") {

    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }
#else
TEST_CASE("EtcdTests_boost") {
#endif

    SECTION("v2") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->createManager();

        std::thread t([&]() {
#ifdef TEST_URING
            REQUIRE(queue->createEventLoop());
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, 10);
#ifdef ICHOR_USE_SPDLOG
            dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
            dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            dm.createServiceManager<ClientFactory<HTTPCONNIMPL, IHttpConnectionService>, IClientFactory<IHttpConnectionService>>();
#ifdef TEST_URING
            dm.createServiceManager<ClientFactory<CONNIMPL<IClientConnectionService>, IClientConnectionService>, IClientFactory<IClientConnectionService>>();
#endif
            dm.createServiceManager<Etcdv2::v1::EtcdService, Etcdv2::v1::IEtcd>(Properties{{"Address", Ichor::v1::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::v1::make_any<uint16_t>(static_cast<uint16_t>(2379))}, {"TimeoutMs", Ichor::v1::make_any<uint64_t>(1'000ul)}, {"Debug", Ichor::v1::make_any<bool>(true)}});
            dm.createServiceManager<Etcdv2UsingService>(Properties{{"LogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            dm.createServiceManager<TimerFactoryFactory>();

            queue->start(CaptureSigInt);
        });

        t.join();
    }

    SECTION("v3") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->createManager();

        std::thread t([&]() {
#ifdef TEST_URING
            REQUIRE(queue->createEventLoop());
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, 10);
#ifdef ICHOR_USE_SPDLOG
            dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
            dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            dm.createServiceManager<ClientFactory<HTTPCONNIMPL, IHttpConnectionService>, IClientFactory<IHttpConnectionService>>();
#ifdef TEST_URING
            dm.createServiceManager<ClientFactory<CONNIMPL<IClientConnectionService>, IClientConnectionService>, IClientFactory<IClientConnectionService>>();
#endif
            dm.createServiceManager<Etcdv3::v1::EtcdService, Etcdv3::v1::IEtcd>(Properties{{"Address", Ichor::v1::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::v1::make_any<uint16_t>(static_cast<uint16_t>(2379))}, {"TimeoutMs", Ichor::v1::make_any<uint64_t>(1'000ul)}, {"Debug", Ichor::v1::make_any<bool>(true)}});
            dm.createServiceManager<Etcdv3UsingService>(Properties{{"LogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            dm.createServiceManager<TimerFactoryFactory>();

            queue->start(CaptureSigInt);
        });

        t.join();
    }
}
