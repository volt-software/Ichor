#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/redis/IRedis.h>

namespace Ichor {
    struct IRedisUsingService {
    };

    struct RedisUsingService final : public IRedisUsingService, public AdvancedService<RedisUsingService> {
        RedisUsingService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
            reg.registerDependency<IRedis>(this, true);
        }

        Task<tl::expected<void, Ichor::StartError>> start() final {
            {
                co_await _redis->set("test_key", "test_value").begin();
                RedisGetReply &reply = *co_await _redis->get("test_key").begin();

                if (!reply.value || *reply.value != "test_value") {
                    throw std::runtime_error("Incorrect value");
                }
            }
            {
                co_await _redis->set("delete_key", "delete").begin();
                RedisIntegerReply &reply = *co_await _redis->del("delete_key").begin();

                if (reply.value != 1) {
                    throw std::runtime_error("Incorrect value");
                }
            }
            {
                co_await _redis->set("integer_key", "10").begin();
                RedisIntegerReply &reply = *co_await _redis->incr("integer_key").begin();

                if (reply.value != 11) {
                    throw std::runtime_error("Incorrect value");
                }
            }
            {
                co_await _redis->set("integer_key", "10").begin();
                RedisIntegerReply &reply = *co_await _redis->incrBy("integer_key", 10).begin();

                if (reply.value != 20) {
                    throw std::runtime_error("Incorrect value");
                }
            }
            {
                co_await _redis->set("integer_key", "10").begin();
                RedisIntegerReply &reply = *co_await _redis->decr("integer_key").begin();

                if (reply.value != 9) {
                    throw std::runtime_error("Incorrect value");
                }
            }
            {
                co_await _redis->set("integer_key", "10").begin();
                RedisIntegerReply &reply = *co_await _redis->decrBy("integer_key", 10).begin();

                if (reply.value != 0) {
                    throw std::runtime_error("Incorrect value");
                }
            }

            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

            co_return {};
        }

        void addDependencyInstance(IRedis &redis, IService&) {
            _redis = &redis;
        }

        void removeDependencyInstance(IRedis&, IService&) {
            _redis = nullptr;
        }

        IRedis *_redis;
    };
}
