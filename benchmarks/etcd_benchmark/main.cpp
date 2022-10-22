#include "UsingEtcdService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/optional_bundles/logging_bundle/LoggerAdmin.h>
#include <ichor/optional_bundles/etcd_bundle/EtcdService.h>
#include <ichor/CommunicationChannel.h>
#ifdef ICHOR_USE_SPDLOG
#include <ichor/optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <ichor/optional_bundles/logging_bundle/SpdlogLogger.h>

#define FRAMEWORK_LOGGER_TYPE SpdlogFrameworkLogger
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <ichor/optional_bundles/logging_bundle/CoutLogger.h>

#define FRAMEWORK_LOGGER_TYPE CoutFrameworkLogger
#define LOGGER_TYPE CoutLogger
#endif
#include <string>

int main() {
    using namespace std::string_literals;
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::steady_clock::now();

    CommunicationChannel channel{};
    auto queueOne = std::make_unique<MultimapQueue>();
    auto &dmOne = queueOne->createManager();
    auto queueTwo = std::make_unique<MultimapQueue>();
    auto &dmTwo = queueTwo->createManager();
    channel.addManager(&dmOne);
    channel.addManager(&dmTwo);

    std::thread t1([&] {
        auto logMgr = dmOne.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
        logMgr->setLogLevel(LogLevel::INFO);
#ifdef ICHOR_USE_SPDLOG
        dmOne.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
        dmOne.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
        dmOne.createServiceManager<EtcdService, IEtcdService>(Properties{{"EtcdAddress", Ichor::make_any<std::string, std::string>("localhost:2379")}});
        dmOne.createServiceManager<UsingEtcdService>();
        queueOne->start(CaptureSigInt);
    });

    std::thread t2([&] {
        auto logMgr = dmTwo.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
        logMgr->setLogLevel(LogLevel::INFO);
#ifdef ICHOR_USE_SPDLOG
        dmTwo.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
        dmTwo.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
        dmTwo.createServiceManager<EtcdService, IEtcdService>(Properties{{"EtcdAddress", Ichor::make_any<std::string, std::string>("localhost:2379")}});
        dmTwo.createServiceManager<UsingEtcdService>();
        queueTwo->start(CaptureSigInt);
    });

    t1.join();
    t2.join();

    auto end = std::chrono::steady_clock::now();
    fmt::print("Program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}