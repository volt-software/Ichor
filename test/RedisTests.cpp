#ifdef ICHOR_USE_HIREDIS

#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/dependency_management/ConstructorInjectionService.h>
#include <ichor/services/logging/LoggerAdmin.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <ichor/services/logging/CoutLogger.h>
#include <ichor/services/redis/HiredisService.h>
#include "TestServices/RedisUsingService.h"
#include "Common.h"

using namespace Ichor;

TEST_CASE("RedisTests") {
    SECTION("Set/get") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, 10);
            dm.createServiceManager<LoggerAdmin<CoutLogger>, ILoggerAdmin>();
            dm.createServiceManager<HiredisService, IRedis>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(6379))}});
            dm.createServiceManager<RedisUsingService>();

            queue->start(CaptureSigInt);
        });

        t.join();
    }
}

#endif

