#include "OneService.h"
#include "OtherService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerFactory.h>

#ifdef ICHOR_USE_SPDLOG
#include <ichor/services/logging/SpdlogLogger.h>
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/services/logging/CoutLogger.h>
#define LOGGER_TYPE CoutLogger
#endif

#include <ichor/CommunicationChannel.h>
#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char *argv[]) {
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
#ifdef ICHOR_USE_SPDLOG
        dmOne.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
        dmOne.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>()->setDefaultLogLevel(Ichor::LogLevel::LOG_INFO);
        dmOne.createServiceManager<OneService>();
        queueOne->start(CaptureSigInt);
    });

    std::thread t2([&] {
#ifdef ICHOR_USE_SPDLOG
        dmTwo.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
        dmTwo.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>()->setDefaultLogLevel(Ichor::LogLevel::LOG_INFO);
        dmTwo.createServiceManager<OtherService>();
        queueTwo->start(CaptureSigInt);
    });

    t1.join();
    t2.join();

    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}
