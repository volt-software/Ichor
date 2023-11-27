#pragma once

#ifdef ICHOR_USE_HIREDIS

#include <ichor/services/redis/IRedis.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <hiredis/hiredis.h>

namespace Ichor {
    /**
     * Service for the redis protocol.
     *
     * Properties:
     * - "PollIntervalMs" - with which interval in milliseconds to poll hiredis for responses. Lower values reduce latency at the cost of more CPU usage.
     * - "TryConnectIntervalMs" - with which interval in milliseconds to try (re)connecting
     * - "TimeoutMs" - with which interval in milliseconds to timeout for (re)connecting, after which the service stops itself
     */
    class HiredisService final : public IRedis, public AdvancedService<HiredisService> {
    public:
        HiredisService(DependencyRegister &reg, Properties props);
        ~HiredisService() override = default;

        void onRedisConnect(int status);
        void onRedisDisconnect(int status);

        // see IRedis for function descriptions
        AsyncGenerator<tl::expected<RedisAuthReply, RedisError>> auth(std::string_view user, std::string_view password) final;
        AsyncGenerator<tl::expected<RedisSetReply, RedisError>> set(std::string_view key, std::string_view value) final;
        AsyncGenerator<tl::expected<RedisSetReply, RedisError>> set(std::string_view key, std::string_view value, RedisSetOptions const &opts) final;
        AsyncGenerator<tl::expected<RedisGetReply, RedisError>> get(std::string_view key) final;
        AsyncGenerator<tl::expected<RedisIntegerReply, RedisError>> del(std::string_view keys) final;
        AsyncGenerator<tl::expected<RedisIntegerReply, RedisError>> incr(std::string_view keys) final;
        AsyncGenerator<tl::expected<RedisIntegerReply, RedisError>> incrBy(std::string_view keys, int64_t incr) final;
        AsyncGenerator<tl::expected<RedisIntegerReply, RedisError>> incrByFloat(std::string_view keys, double incr) final;
        AsyncGenerator<tl::expected<RedisIntegerReply, RedisError>> decr(std::string_view keys) final;
        AsyncGenerator<tl::expected<RedisIntegerReply, RedisError>> decrBy(std::string_view keys, int64_t decr) final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService&);
        void removeDependencyInstance(ILogger &logger, IService&);

        void addDependencyInstance(ITimerFactory &logger, IService&);
        void removeDependencyInstance(ITimerFactory &logger, IService&);

        void addDependencyInstance(IEventQueue &queue, IService&);
        void removeDependencyInstance(IEventQueue &queue, IService&);

        tl::expected<void, Ichor::StartError> connect();

        friend DependencyRegister;

        ILogger *_logger{};
        redisAsyncContext *_redisContext{};
        AsyncManualResetEvent _disconnectEvt{};
        ITimerFactory *_timerFactory{};
        IEventQueue *_queue{};
        ITimer *_timeoutTimer{};
        uint64_t _pollIntervalMs{10};
        uint64_t _tryConnectIntervalMs{10};
        uint64_t _timeoutMs{10'000};
        uint64_t _timeWhenDisconnected{};
    };
}

#endif
