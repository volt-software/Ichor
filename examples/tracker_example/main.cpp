#include "TestService.h"
#include "TrackerService.h"
#include "RuntimeCreatedService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerFactory.h>

#ifdef ICHOR_USE_SPDLOG
#include <ichor/services/logging/SpdlogLogger.h>
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/services/logging/CoutLogger.h>
#define LOGGER_TYPE CoutLogger
#endif

#include <chrono>
#include <iostream>

using namespace std::string_literals;

int main(int argc, char *argv[]) {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::steady_clock::now();
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
#ifdef ICHOR_USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
    dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>()->setDefaultLogLevel(Ichor::LogLevel::LOG_INFO);
    dm.createServiceManager<TestService>(Properties{{"scope", Ichor::make_any<std::string>("one"s)}});
    dm.createServiceManager<TestService>(Properties{{"scope", Ichor::make_any<std::string>("two"s)}});
    dm.createServiceManager<TrackerService>();
    queue->start(CaptureSigInt);
    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}
