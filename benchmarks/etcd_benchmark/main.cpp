#include "UsingEtcdService.h"
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#include <optional_bundles/etcd_bundle/EtcdService.h>
#include <framework/CommunicationChannel.h>
#ifdef USE_SPDLOG
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <optional_bundles/logging_bundle/SpdlogLogger.h>

#define FRAMEWORK_LOGGER_TYPE SpdlogFrameworkLogger
#define LOGGER_TYPE SpdlogLogger
#define LOGGER_SHARED_TYPE ,ISpdlogSharedService
#else
#include <optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <optional_bundles/logging_bundle/CoutLogger.h>

#define FRAMEWORK_LOGGER_TYPE CoutFrameworkLogger
#define LOGGER_TYPE CoutLogger
#define LOGGER_SHARED_TYPE
#endif
#include <string>

int main() {
    using namespace std::string_literals;
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::system_clock::now();

    CommunicationChannel channel{};
    DependencyManager dmOne{};
    DependencyManager dmTwo{};
    channel.addManager(&dmOne);
    channel.addManager(&dmTwo);

    std::thread t1([&dmOne] {
        auto logMgr = dmOne.createServiceManager<IFrameworkLogger, FRAMEWORK_LOGGER_TYPE>();
        logMgr->setLogLevel(LogLevel::INFO);
#ifdef USE_SPDLOG
        dmOne.createServiceManager<ISpdlogSharedService, SpdlogSharedService>();
#endif
        dmOne.createServiceManager<ILoggerAdmin, LoggerAdmin<LOGGER_TYPE LOGGER_SHARED_TYPE>>(RequiredList<IFrameworkLogger>);
        dmOne.createServiceManager<IEtcdService, EtcdService>(RequiredList<ILogger>, OptionalList<>, CppelixProperties{{"EtcdAddress", "localhost:2379"s}});
        dmOne.createServiceManager<IUsingEtcdService, UsingEtcdService>(RequiredList<ILogger, IEtcdService>, OptionalList<>);
        dmOne.start();
    });

    std::thread t2([&dmTwo] {
        auto logMgr = dmTwo.createServiceManager<IFrameworkLogger, FRAMEWORK_LOGGER_TYPE>();
        logMgr->setLogLevel(LogLevel::INFO);
#ifdef USE_SPDLOG
        dmTwo.createServiceManager<ISpdlogSharedService, SpdlogSharedService>();
#endif
        dmTwo.createServiceManager<ILoggerAdmin, LoggerAdmin<LOGGER_TYPE LOGGER_SHARED_TYPE>>(RequiredList<IFrameworkLogger>);
        dmTwo.createServiceManager<IEtcdService, EtcdService>(RequiredList<ILogger>, OptionalList<>, CppelixProperties{{"EtcdAddress", "localhost:2379"s}});
        dmTwo.createServiceManager<IUsingEtcdService, UsingEtcdService>(RequiredList<ILogger, IEtcdService>, OptionalList<>);
        dmTwo.start();
    });

    t1.join();
    t2.join();

    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}