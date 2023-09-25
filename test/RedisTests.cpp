#ifdef ICHOR_USE_HIREDIS

#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <ichor/services/logging/CoutLogger.h>
#include <ichor/services/redis/HiredisService.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include "TestServices/RedisUsingService.h"
#include "Common.h"

using namespace Ichor;

TEST_CASE("RedisTests") {
    SECTION("Set/get") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, 10);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            dm.createServiceManager<HiredisService, IRedis>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(6379))}, {"PollIntervalMs", Ichor::make_any<uint64_t>(1ul)}});
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

    }
}

#endif

