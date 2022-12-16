#pragma once

#include <ichor/Service.h>
#include <ichor/services/redis/IRedis.h>

namespace Ichor {
    struct IRedisUsingService {
    };

    struct RedisUsingService final : public IRedisUsingService, public Service<RedisUsingService> {
        RedisUsingService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
            reg.registerDependency<IRedis>(this, true);
        }

        AsyncGenerator<void> start() final {
            co_await _redis->set("test_key", "test_value").begin();
            RedisGetReply &reply = *co_await _redis->get("test_key").begin();

            if(!reply.value || *reply.value != "test_value") {
                throw std::runtime_error("Incorrect value");
            }

            getManager().pushEvent<QuitEvent>(getServiceId());

            co_return;
        }

        void addDependencyInstance(IRedis *redis, IService *) {
            _redis = redis;
        }

        void removeDependencyInstance(IRedis *, IService *) {
            _redis = nullptr;
        }

        IRedis *_redis;
    };
}
