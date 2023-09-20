#pragma once

#ifdef ICHOR_USE_HIREDIS

#include <ichor/services/redis/IRedis.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <hiredis/hiredis.h>

namespace Ichor {
    class HiredisService final : public IRedis, public AdvancedService<HiredisService> {
    public:
        HiredisService(DependencyRegister &reg, Properties props);
        ~HiredisService() override = default;

        void onRedisConnect(int status);
        void onRedisDisconnect(int status);

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

        // see IRedis for function descriptions
        AsyncGenerator<RedisAuthReply> auth(std::string_view user, std::string_view password) final;
        AsyncGenerator<RedisSetReply> set(std::string_view key, std::string_view value) final;
        AsyncGenerator<RedisSetReply> set(std::string_view key, std::string_view value, RedisSetOptions const &opts) final;
        void setAndForget(std::string_view key, std::string_view value) final;
        void setAndForget(std::string_view key, std::string_view value, RedisSetOptions const &opts) final;
        AsyncGenerator<RedisGetReply> get(std::string_view key) final;
        AsyncGenerator<RedisIntegerReply> del(std::string_view keys) final;
        AsyncGenerator<RedisIntegerReply> incr(std::string_view keys) final;
        AsyncGenerator<RedisIntegerReply> incrBy(std::string_view keys, int64_t incr) final;
        AsyncGenerator<RedisIntegerReply> incrByFloat(std::string_view keys, double incr) final;
        AsyncGenerator<RedisIntegerReply> decr(std::string_view keys) final;
        AsyncGenerator<RedisIntegerReply> decrBy(std::string_view keys, int64_t decr) final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService&);
        void removeDependencyInstance(ILogger &logger, IService&);

        void addDependencyInstance(ITimerFactory &logger, IService&);
        void removeDependencyInstance(ITimerFactory &logger, IService&);

        tl::expected<void, Ichor::StartError> connect();

        friend DependencyRegister;

        ILogger *_logger{};
        std::atomic<uint64_t> _priority{INTERNAL_EVENT_PRIORITY};
        redisAsyncContext *_redisContext{};
        AsyncManualResetEvent _disconnectEvt{};
        ITimerFactory *_timerFactory{};
        uint64_t _pollIntervalMs{10};
    };
}

#endif
