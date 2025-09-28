#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/redis/IRedis.h>
#include <ichor/ServiceExecutionScope.h>

using namespace Ichor::v1;

namespace Ichor {
    struct RedisUsingService final : public AdvancedService<RedisUsingService> {
        RedisUsingService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
            reg.registerDependency<IRedis>(this, DependencyFlags::REQUIRED);
            reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
        }

        Task<tl::expected<void, Ichor::StartError>> start() final {
            co_await version_test();
            co_await set_test();
            co_await strlen_test();
            co_await delete_test();
            co_await get_delete_test();
            co_await incr_test();
            co_await incrBy_test();
            co_await decr_test();
            co_await decrBy_test();
            co_await transaction_test();

            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

            co_return {};
        }

        void addDependencyInstance(Ichor::ScopedServiceProxy<IRedis*> redis, IService&) {
            _redis = std::move(redis);
        }

        void removeDependencyInstance(Ichor::ScopedServiceProxy<IRedis*>, IService&) {
            _redis.reset();
        }

        void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService&) {
            _logger = std::move(logger);
        }

        void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*>, IService&) {
            _logger.reset();
        }

        Task<void> version_test() {
            ICHOR_LOG_INFO(_logger, "running test");
            auto versionReply = co_await _redis->getServerVersion();
            if(!versionReply) {
                fmt::println("version");
                std::terminate();
            }
            _v = versionReply.value();
        }

        Task<void> set_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto setReply = co_await _redis->set("test_key", "test_value");
            if(!setReply) {
                fmt::println("set");
                std::terminate();
            }
            auto getReply = co_await _redis->get("test_key");
            if(!getReply) {
                fmt::println("get");
                std::terminate();
            }

            if (!getReply.value().value || *getReply.value().value != "test_value") {
                fmt::println("Incorrect value");
                std::terminate();
            }
        }

        Task<void> strlen_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto strlenReply = co_await _redis->strlen("test_key");
            if(!strlenReply) {
                fmt::println("strlen");
                std::terminate();
            }

            if (strlenReply.value().value != 10) {
                fmt::println("Incorrect value");
                std::terminate();
            }
        }

        Task<void> delete_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto setReply = co_await _redis->set("delete_key", "delete");
            if(!setReply) {
                fmt::println("set");
                std::terminate();
            }
            auto delReply= co_await _redis->del("delete_key");
            if(!delReply) {
                fmt::println("del");
                std::terminate();
            }

            if (delReply.value().value != 1) {
                fmt::println("Incorrect value delReply");
                std::terminate();
            }
        }

        Task<void> get_delete_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            if(_v >= Version{6, 2, 0}) {
                auto setReply = co_await _redis->set("get_delete_key", "get_delete");
                if(!setReply) {
                    fmt::println("set");
                    std::terminate();
                }
                auto delReply= co_await _redis->getdel("get_delete_key");
                if(!delReply) {
                    fmt::println("getdel");
                    std::terminate();
                }

                if (!delReply.value().value || *delReply.value().value != "get_delete") {
                    fmt::println("Incorrect value get_delete");
                    std::terminate();
                }
                auto getReply = co_await _redis->get("get_delete_key");
                if(getReply && getReply->value.has_value()) {
                    fmt::println("get");
                    std::terminate();
                }
            } else {
                auto delReply = co_await _redis->getdel("get_delete_key");
                if(delReply || delReply.error() != RedisError::FUNCTION_NOT_AVAILABLE_IN_SERVER) {
                    fmt::println("getdel");
                    std::terminate();
                }
            }
        }

        Task<void> incr_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto setReply = co_await _redis->set("integer_key", "10");
            if(!setReply) {
                fmt::println("set");
                std::terminate();
            }
            auto incrReply= co_await _redis->incr("integer_key");
            if(!incrReply) {
                fmt::println("incr");
                std::terminate();
            }

            if (incrReply.value().value != 11) {
                fmt::println("Incorrect value");
                std::terminate();
            }
        }

        Task<void> incrBy_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto setReply = co_await _redis->set("integer_key", "10");
            if(!setReply) {
                fmt::println("set");
                std::terminate();
            }
            auto incrByReply= co_await _redis->incrBy("integer_key", 10);
            if(!incrByReply) {
                fmt::println("incrBy");
                std::terminate();
            }

            if (incrByReply.value().value != 20) {
                fmt::println("Incorrect value");
                std::terminate();
            }
        }

        Task<void> decr_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto setReply = co_await _redis->set("integer_key", "10");
            if(!setReply) {
                fmt::println("set");
                std::terminate();
            }
            auto decrReply = co_await _redis->decr("integer_key");
            if(!decrReply) {
                fmt::println("decr");
                std::terminate();
            }

            if (decrReply.value().value != 9) {
                fmt::println("Incorrect value");
                std::terminate();
            }
        }

        Task<void> decrBy_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto setReply = co_await _redis->set("integer_key", "10");
            if(!setReply) {
                fmt::println("set");
                std::terminate();
            }
            auto decrByReply= co_await _redis->decrBy("integer_key", 10);
            if(!decrByReply) {
                fmt::println("decrBy");
                std::terminate();
            }

            if (decrByReply.value().value != 0) {
                fmt::println("Incorrect value");
                std::terminate();
            }
        }

        Task<void> transaction_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto multiReply = co_await _redis->multi();
            if(!multiReply) {
                fmt::println("multi");
                std::terminate();
            }
            auto multi2Reply = co_await _redis->multi();
            if(multi2Reply) {
                fmt::println("multi2");
                std::terminate();
            }

            auto setReply = co_await _redis->set("multi_key", "10");
            if(setReply || setReply.error() != RedisError::QUEUED) {
                fmt::println("set");
                std::terminate();
            }
            auto getReply = co_await _redis->get("multi_key");
            if(getReply || getReply.error() != RedisError::QUEUED) {
                fmt::println("get");
                std::terminate();
            }

            auto execReply = co_await _redis->exec();
            if(!execReply) {
                fmt::println("exec");
                std::terminate();
            }

            if(execReply.value().size() != 2) {
                fmt::println("exec size");
                std::terminate();
            }

            if(std::get<RedisGetReply>(execReply.value()[1]).value != "10") {
                fmt::println("exec value");
                std::terminate();
            }

            auto discardReply = co_await _redis->discard();
            if(discardReply) {
                fmt::println("discard");
                std::terminate();
            }
        }


        Ichor::ScopedServiceProxy<IRedis*> _redis {};
        Ichor::ScopedServiceProxy<ILogger*> _logger {};
        Version _v{};
    };
}
