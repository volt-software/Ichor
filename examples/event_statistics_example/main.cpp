#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/NullFrameworkLogger.h>
#include <ichor/services/metrics/EventStatisticsService.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/ichor-mimalloc.h>
#include "UsingStatisticsService.h"

// Some compile time logic to instantiate a regular cout logger or to use the spdlog logger, if Ichor has been compiled with it.
#ifdef ICHOR_USE_SPDLOG
#include <ichor/services/logging/SpdlogLogger.h>
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/services/logging/CoutLogger.h>
#define LOGGER_TYPE CoutLogger
#endif

#include <chrono>
#include <iostream>

using namespace Ichor;
using namespace std::string_literals;

int main(int argc, char *argv[]) {
#if ICHOR_EXCEPTIONS_ENABLED
    try {
#endif
        std::locale::global(std::locale("en_US.UTF-8"));
#if ICHOR_EXCEPTIONS_ENABLED
    } catch(std::runtime_error const &e) {
        fmt::println("Couldn't set locale to en_US.UTF-8: {}", e.what());
    }
#endif

    auto start = std::chrono::steady_clock::now();
    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->createManager();

    uint64_t priorityToEnsureStartingFirst = 51;

#ifdef ICHOR_USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>(Properties{}, priorityToEnsureStartingFirst);
#endif
    dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
    dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_INFO)}}, priorityToEnsureStartingFirst);
    dm.createServiceManager<EventStatisticsService, IEventStatisticsService>(Properties{{"ShowStatisticsOnStop", make_any<bool>(true)}}, priorityToEnsureStartingFirst);
    dm.createServiceManager<UsingStatisticsService>();
    dm.createServiceManager<TimerFactoryFactory>(Properties{}, priorityToEnsureStartingFirst);
    queue->start(CaptureSigInt);
    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}
