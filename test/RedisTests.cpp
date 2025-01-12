#ifdef ICHOR_USE_HIREDIS

#include "Common.h"
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/redis/HiredisService.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include "TestServices/RedisUsingService.h"

#ifdef ICHOR_USE_SPDLOG
#include <ichor/services/logging/SpdlogLogger.h>
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/services/logging/CoutLogger.h>
#define LOGGER_TYPE CoutLogger
#endif

using namespace Ichor;

TEST_CASE("RedisTests") {
    SECTION("Set/get") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>(Properties{{"LogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, 10);
#ifdef ICHOR_USE_SPDLOG
            dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
            dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            dm.createServiceManager<HiredisService, IRedis>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(6379))}, {"PollIntervalMs", Ichor::make_any<uint64_t>(1ul)}, {"TimeoutMs", Ichor::make_any<uint64_t>(1'000ul)}, {"Debug", Ichor::make_any<bool>(true)}});
            dm.createServiceManager<RedisUsingService>();
            dm.createServiceManager<TimerFactoryFactory>();

            queue->start(CaptureSigInt);
        });

        t.join();
    }
}

#else

#include "Common.h"

TEST_CASE("RedisTests") {
    SECTION("Empty Test so that Catch2 exits with 0") {
        REQUIRE(true);
    }
}

#endif

