#pragma once

#ifdef ICHOR_USE_HIREDIS

#include <ichor/services/redis/IRedis.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/stl/StringUtils.h>
#include <hiredis/hiredis.h>
#include <ichor/ScopedServiceProxy.h>

namespace Ichor::v1 {
    /**
     * Service for the redis protocol.
     *
     * Properties:
     * - "Address" std::string - What address to connect to (required)
     * - "Port" uint16_t - What port to connect to (required)
     * - "PollIntervalMs" uint64_t - with which interval in milliseconds to poll hiredis for responses. Lower values reduce latency at the cost of more CPU usage. (default: 10 ms)
     * - "TryConnectIntervalMs" uint64_t - with which interval in milliseconds to try (re)connecting (default: 100 ms)
     * - "TimeoutMs" uint64_t - with which interval in milliseconds to timeout for (re)connecting, after which the service stops itself (default: 10'000 ms)
     * - "Debug" bool - Enable verbose logging of redis requests and responses (default: false)
     */
    class HiredisService final : public IRedis, public AdvancedService<HiredisService> {
    public:
        HiredisService(DependencyRegister &reg, Properties props);
        ~HiredisService() override = default;

        void onRedisConnect(int status);
        void onRedisDisconnect(int status);

        void setDebug(bool debug) noexcept;
        [[nodiscard]] bool getDebug() const noexcept;

        // see IRedis for function descriptions
        Task<tl::expected<RedisAuthReply, RedisError>> auth(std::string_view user, std::string_view password) final;
        Task<tl::expected<RedisSetReply, RedisError>> set(std::string_view key, std::string_view value) final;
        Task<tl::expected<RedisSetReply, RedisError>> set(std::string_view key, std::string_view value, RedisSetOptions const &opts) final;
        Task<tl::expected<RedisGetReply, RedisError>> get(std::string_view key) final;
        Task<tl::expected<RedisGetReply, RedisError>> getdel(std::string_view key) final;
        Task<tl::expected<RedisIntegerReply, RedisError>> del(std::string_view keys) final;
        Task<tl::expected<RedisIntegerReply, RedisError>> incr(std::string_view keys) final;
        Task<tl::expected<RedisIntegerReply, RedisError>> incrBy(std::string_view keys, int64_t incr) final;
        Task<tl::expected<RedisIntegerReply, RedisError>> incrByFloat(std::string_view keys, double incr) final;
        Task<tl::expected<RedisIntegerReply, RedisError>> decr(std::string_view keys) final;
        Task<tl::expected<RedisIntegerReply, RedisError>> decrBy(std::string_view keys, int64_t decr) final;
        Task<tl::expected<RedisIntegerReply, RedisError>> strlen(std::string_view key) final;
        Task<tl::expected<void, RedisError>> multi() final;
        Task<tl::expected<std::vector<std::variant<RedisGetReply, RedisSetReply, RedisAuthReply, RedisIntegerReply>>, RedisError>> exec() final;
        Task<tl::expected<void, RedisError>> discard() final;
        Task<tl::expected<std::unordered_map<std::string, std::string>, RedisError>> info() final;
        Task<tl::expected<Version, RedisError>> getServerVersion() final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService&);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService&);

        void addDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> logger, IService&);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> logger, IService&);

        void addDependencyInstance(Ichor::ScopedServiceProxy<IEventQueue*> queue, IService&);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<IEventQueue*> queue, IService&);

        tl::expected<void, Ichor::StartError> connect(std::string const &addr, uint16_t port);

        friend DependencyRegister;

        Ichor::ScopedServiceProxy<ILogger*> _logger {};
        redisAsyncContext *_redisContext{};
        AsyncManualResetEvent _startEvt{};
        AsyncManualResetEvent _disconnectEvt{};
        Ichor::ScopedServiceProxy<ITimerFactory*> _timerFactory {};
        Ichor::ScopedServiceProxy<IEventQueue*> _queue {};
        tl::optional<TimerRef> _timeoutTimer{};
        std::vector<NameHashType> _queuedResponseTypes{};
        bool _timeoutTimerRunning{};
        bool _debug{};
        tl::optional<Version> _redisVersion{};
        uint64_t _pollIntervalMs{10};
        uint64_t _tryConnectIntervalMs{100};
        uint64_t _timeoutMs{10'000};
        uint64_t _timeWhenDisconnected{};
    };
}

#endif
