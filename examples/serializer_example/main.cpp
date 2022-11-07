#include "TestService.h"
#include "../common/TestMsgJsonSerializer.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerAdmin.h>
#include <ichor/services/serialization/ISerializer.h>
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
#include <chrono>
#include <iostream>

int main(int argc, char *argv[]) {
    std::locale::global(std::locale("en_US.UTF-8"));
    std::ios::sync_with_stdio(false);

    auto start = std::chrono::steady_clock::now();
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
    dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
#ifdef ICHOR_USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
    dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
    dm.createServiceManager<TestMsgJsonSerializer, ISerializer<TestMsg>>();
    dm.createServiceManager<TestService>();
    queue->start(CaptureSigInt);
    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}