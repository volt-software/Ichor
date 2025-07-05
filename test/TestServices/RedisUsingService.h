#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/redis/IRedis.h>

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

        void addDependencyInstance(IRedis &redis, IService&) {
            _redis = &redis;
        }

        void removeDependencyInstance(IRedis&, IService&) {
            _redis = nullptr;
        }

        void addDependencyInstance(ILogger &logger, IService&) {
            _logger = &logger;
        }

        void removeDependencyInstance(ILogger &, IService&) {
            _logger = nullptr;
        }

        Task<void> version_test() {
            ICHOR_LOG_INFO(_logger, "running test");
            auto versionReply = co_await _redis->getServerVersion();
            if(!versionReply) {
                throw std::runtime_error("version");
            }
            _v = versionReply.value();
        }

        Task<void> set_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto setReply = co_await _redis->set("test_key", "test_value");
            if(!setReply) {
                throw std::runtime_error("set");
            }
            auto getReply = co_await _redis->get("test_key");
            if(!getReply) {
                throw std::runtime_error("get");
            }

            if (!getReply.value().value || *getReply.value().value != "test_value") {
                throw std::runtime_error("Incorrect value");
            }
        }

        Task<void> strlen_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto strlenReply = co_await _redis->strlen("test_key");
            if(!strlenReply) {
                throw std::runtime_error("strlen");
            }

            if (strlenReply.value().value != 10) {
                throw std::runtime_error("Incorrect value");
            }
        }

        Task<void> delete_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto setReply = co_await _redis->set("delete_key", "delete");
            if(!setReply) {
                throw std::runtime_error("set");
            }
            auto delReply= co_await _redis->del("delete_key");
            if(!delReply) {
                throw std::runtime_error("del");
            }

            if (delReply.value().value != 1) {
                throw std::runtime_error("Incorrect value delReply");
            }
        }

        Task<void> get_delete_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            if(_v >= Version{6, 2, 0}) {
                auto setReply = co_await _redis->set("get_delete_key", "get_delete");
                if(!setReply) {
                    throw std::runtime_error("set");
                }
                auto delReply= co_await _redis->getdel("get_delete_key");
                if(!delReply) {
                    throw std::runtime_error("getdel");
                }

                if (!delReply.value().value || *delReply.value().value != "get_delete") {
                    throw std::runtime_error("Incorrect value get_delete");
                }
                auto getReply = co_await _redis->get("get_delete_key");
                if(getReply && getReply->value.has_value()) {
                    throw std::runtime_error("get");
                }
            } else {
                auto delReply = co_await _redis->getdel("get_delete_key");
                if(delReply || delReply.error() != RedisError::FUNCTION_NOT_AVAILABLE_IN_SERVER) {
                    throw std::runtime_error("getdel");
                }
            }
        }

        Task<void> incr_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto setReply = co_await _redis->set("integer_key", "10");
            if(!setReply) {
                throw std::runtime_error("set");
            }
            auto incrReply= co_await _redis->incr("integer_key");
            if(!incrReply) {
                throw std::runtime_error("incr");
            }

            if (incrReply.value().value != 11) {
                throw std::runtime_error("Incorrect value");
            }
        }

        Task<void> incrBy_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto setReply = co_await _redis->set("integer_key", "10");
            if(!setReply) {
                throw std::runtime_error("set");
            }
            auto incrByReply= co_await _redis->incrBy("integer_key", 10);
            if(!incrByReply) {
                throw std::runtime_error("incrBy");
            }

            if (incrByReply.value().value != 20) {
                throw std::runtime_error("Incorrect value");
            }
        }

        Task<void> decr_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto setReply = co_await _redis->set("integer_key", "10");
            if(!setReply) {
                throw std::runtime_error("set");
            }
            auto decrReply = co_await _redis->decr("integer_key");
            if(!decrReply) {
                throw std::runtime_error("decr");
            }

            if (decrReply.value().value != 9) {
                throw std::runtime_error("Incorrect value");
            }
        }

        Task<void> decrBy_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto setReply = co_await _redis->set("integer_key", "10");
            if(!setReply) {
                throw std::runtime_error("set");
            }
            auto decrByReply= co_await _redis->decrBy("integer_key", 10);
            if(!decrByReply) {
                throw std::runtime_error("decrBy");
            }

            if (decrByReply.value().value != 0) {
                throw std::runtime_error("Incorrect value");
            }
        }

        Task<void> transaction_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            auto multiReply = co_await _redis->multi();
            if(!multiReply) {
                throw std::runtime_error("multi");
            }
            auto multi2Reply = co_await _redis->multi();
            if(multi2Reply) {
                throw std::runtime_error("multi2");
            }

            auto setReply = co_await _redis->set("multi_key", "10");
            if(setReply || setReply.error() != RedisError::QUEUED) {
                throw std::runtime_error("set");
            }
            auto getReply = co_await _redis->get("multi_key");
            if(getReply || getReply.error() != RedisError::QUEUED) {
                throw std::runtime_error("get");
            }

            auto execReply = co_await _redis->exec();
            if(!execReply) {
                throw std::runtime_error("exec");
            }

            if(execReply.value().size() != 2) {
                throw std::runtime_error("exec size");
            }

            if(std::get<RedisGetReply>(execReply.value()[1]).value != "10") {
                throw std::runtime_error("exec value");
            }

            auto discardReply = co_await _redis->discard();
            if(discardReply) {
                throw std::runtime_error("discard");
            }
        }


        IRedis *_redis{};
        ILogger *_logger{};
        Version _v{};
    };
}
