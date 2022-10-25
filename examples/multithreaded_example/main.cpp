#include "OneService.h"
#include "OtherService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerAdmin.h>
#ifdef ICHOR_USE_SPDLOG
#include <ichor/services/logging/SpdlogFrameworkLogger.h>
#include <ichor/services/logging/SpdlogLogger.h>

#define FRAMEWORK_LOGGER_TYPE SpdlogFrameworkLogger
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <ichor/services/logging/CoutLogger.h>

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
    auto queueOne = std::make_unique<MultimapQueue>();
    auto &dmOne = queueOne->createManager();
    auto queueTwo = std::make_unique<MultimapQueue>();
    auto &dmTwo = queueTwo->createManager();
    channel.addManager(&dmOne);
    channel.addManager(&dmTwo);

    std::thread t1([&] {
        dmOne.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
#ifdef ICHOR_USE_SPDLOG
        dmOne.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
        dmOne.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
        dmOne.createServiceManager<OneService>();
        queueOne->start(CaptureSigInt);
    });

    std::thread t2([&] {
        dmTwo.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
#ifdef ICHOR_USE_SPDLOG
        dmTwo.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
        dmTwo.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
        dmTwo.createServiceManager<OtherService>();
        queueTwo->start(CaptureSigInt);
    });

    t1.join();
    t2.join();

    auto end = std::chrono::steady_clock::now();
    fmt::print("Program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}