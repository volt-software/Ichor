#pragma once

#ifdef ICHOR_USE_HIREDIS

#include <ichor/services/redis/IRedis.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/DependencyManager.h>
#include <hiredis/hiredis.h>

namespace Ichor {
    class HiredisService final : public IRedis, public Service<HiredisService> {
    public:
        HiredisService(DependencyRegister &reg, Properties props, DependencyManager *mng);
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

    private:
        AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final;
        AsyncGenerator<void> stop() final;

        void addDependencyInstance(ILogger *logger, IService *);
        void removeDependencyInstance(ILogger *logger, IService *);

        tl::expected<void, Ichor::StartError> connect();

        friend DependencyRegister;
        friend DependencyManager;

        ILogger *_logger{nullptr};
        std::atomic<uint64_t> _priority{INTERNAL_EVENT_PRIORITY};
        redisAsyncContext *_redisContext{};
        AsyncManualResetEvent _disconnectEvt{};
        Timer *_pollTimer{};
    };
}

#endif
