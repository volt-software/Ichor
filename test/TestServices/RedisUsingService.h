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
                auto &setReply = *co_await _redis->set("test_key", "test_value").begin();
                if(!setReply) {
                    throw std::runtime_error("");
                }
                auto &getReply = *co_await _redis->get("test_key").begin();
                if(!getReply) {
                    throw std::runtime_error("");
                }

                if (!getReply.value().value || *getReply.value().value != "test_value") {
                    throw std::runtime_error("Incorrect value");
                }
            }
            {
                auto &setReply = *co_await _redis->set("delete_key", "delete").begin();
                if(!setReply) {
                    throw std::runtime_error("");
                }
                auto &delReply= *co_await _redis->del("delete_key").begin();
                if(!delReply) {
                    throw std::runtime_error("");
                }

                if (delReply.value().value != 1) {
                    throw std::runtime_error("Incorrect value");
                }
            }
            {
                auto &setReply = *co_await _redis->set("integer_key", "10").begin();
                if(!setReply) {
                    throw std::runtime_error("");
                }
                auto &incrReply= *co_await _redis->incr("integer_key").begin();
                if(!incrReply) {
                    throw std::runtime_error("");
                }

                if (incrReply.value().value != 11) {
                    throw std::runtime_error("Incorrect value");
                }
            }
            {
                auto &setReply = *co_await _redis->set("integer_key", "10").begin();
                if(!setReply) {
                    throw std::runtime_error("");
                }
                auto &incrByReply= *co_await _redis->incrBy("integer_key", 10).begin();
                if(!incrByReply) {
                    throw std::runtime_error("");
                }

                if (incrByReply.value().value != 20) {
                    throw std::runtime_error("Incorrect value");
                }
            }
            {
                auto &setReply = *co_await _redis->set("integer_key", "10").begin();
                if(!setReply) {
                    throw std::runtime_error("");
                }
                auto &decrReply = *co_await _redis->decr("integer_key").begin();
                if(!decrReply) {
                    throw std::runtime_error("");
                }

                if (decrReply.value().value != 9) {
                    throw std::runtime_error("Incorrect value");
                }
            }
            {
                auto &setReply = *co_await _redis->set("integer_key", "10").begin();
                if(!setReply) {
                    throw std::runtime_error("");
                }
                auto &decrByReply= *co_await _redis->decrBy("integer_key", 10).begin();
                if(!decrByReply) {
                    throw std::runtime_error("");
                }

                if (decrByReply.value().value != 0) {
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
