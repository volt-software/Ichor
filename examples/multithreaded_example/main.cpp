#include "OneService.h"
#include "OtherService.h"
#include <ichor/optional_bundles/logging_bundle/LoggerAdmin.h>
#ifdef USE_SPDLOG
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
#include <ichor/CommunicationChannel.h>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::steady_clock::now();

    CommunicationChannel channel{};
    DependencyManager dmOne{};
    DependencyManager dmTwo{};
    channel.addManager(&dmOne);
    channel.addManager(&dmTwo);

    std::thread t1([&dmOne] {
        dmOne.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
#ifdef USE_SPDLOG
        dmOne.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
        dmOne.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
        dmOne.createServiceManager<OneService>();
        dmOne.start();
    });

    std::thread t2([&dmTwo] {
        dmTwo.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
#ifdef USE_SPDLOG
        dmTwo.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
        dmTwo.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
        dmTwo.createServiceManager<OtherService>();
        dmTwo.start();
    });

    t1.join();
    t2.join();

    auto end = std::chrono::steady_clock::now();
    fmt::print("Program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}