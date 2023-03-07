#ifdef ICHOR_USE_HIREDIS

#include <ichor/DependencyManager.h>
#include <ichor/services/redis/HiredisService.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/poll.h>
#include <ichor/events/RunFunctionEvent.h>

#define FMT_INLINE_BUFFER_SIZE 1024

namespace Ichor {
    struct IchorRedisReply {
        ~IchorRedisReply() {
            if(reply != nullptr) {
                freeReplyObject(reply);
            }
        }
        redisReply *reply;
        AsyncManualResetEvent evt{};
    };

    static void _onRedisConnect(const struct redisAsyncContext *c, int status) {
        auto *svc = reinterpret_cast<Ichor::HiredisService *>(c->data);
        svc->onRedisConnect(status);
    }

    static void _onRedisDisconnect(const struct redisAsyncContext *c, int status) {
        auto *svc = reinterpret_cast<Ichor::HiredisService *>(c->data);
        svc->onRedisDisconnect(status);
    }

    static void _onAsyncReply(redisAsyncContext *c, void *reply, void *privdata) {
        auto *ichorReply = static_cast<IchorRedisReply*>(privdata);
        auto *svc = static_cast<HiredisService*>(c->data);
        ichorReply->reply = static_cast<redisReply *>(reply);

        GetThreadLocalManager().getEventQueue().pushEvent<RunFunctionEvent>(svc->getServiceId(), [ichorReply](DependencyManager &) {
            ichorReply->evt.set();
        });
    }

    static fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> _formatSet(std::string_view const &key, std::string_view const &value, RedisSetOptions const &opts) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
        fmt::format_to(std::back_inserter(buf), "SET {} {}", key, value);
        uint32_t timeOptsSet{};
        if(opts.NX && opts.XX) [[unlikely]] {
            throw std::runtime_error("Cannot set NX and XX together");
        }
        if(opts.NX) {
            fmt::format_to(std::back_inserter(buf), " NX");
        }
        if(opts.XX) {
            fmt::format_to(std::back_inserter(buf), " XX");
        }
        if(opts.GET) {
            fmt::format_to(std::back_inserter(buf), " GET");
        }
        if(opts.KEEPTTL) {
            timeOptsSet++;
            fmt::format_to(std::back_inserter(buf), " KEEPTTL");
        }
        if(opts.EX.has_value()) {
            timeOptsSet++;
            fmt::format_to(std::back_inserter(buf), " EX {}", *opts.EX);
        }
        if(opts.PX.has_value()) {
            timeOptsSet++;
            fmt::format_to(std::back_inserter(buf), " EX {}", *opts.PX);
        }
        if(opts.EXAT.has_value()) {
            timeOptsSet++;
            fmt::format_to(std::back_inserter(buf), " EX {}", *opts.EXAT);
        }
        if(opts.PXAT.has_value()) {
            timeOptsSet++;
            fmt::format_to(std::back_inserter(buf), " EX {}", *opts.PXAT);
        }
        if(timeOptsSet > 1) [[unlikely]] {
            throw std::runtime_error("Can only set one of EX, PX, EXAT, PXAT and KEEPTTL");
        }
        return buf;
    }
}

Ichor::HiredisService::HiredisService(DependencyRegister &reg, Properties props) : AdvancedService<HiredisService>(std::move(props)) {
    reg.registerDependency<ILogger>(this, true);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::HiredisService::start() {
    if(getProperties().contains("Priority")) {
        _priority = Ichor::any_cast<uint64_t>(getProperties()["Priority"]);
    }

    auto outcome = connect();
    if(!outcome) {
        co_return tl::unexpected(StartError::FAILED);
    }

    _pollTimer = GetThreadLocalManager().createServiceManager<Timer, ITimer>();
    _pollTimer->setCallback(this, [this](DependencyManager &) {
        redisPollTick(_redisContext, 0);
    });
    _pollTimer->setChronoInterval(1ms);
    _pollTimer->startTimer();

    INTERNAL_DEBUG("HiredisService::start() co_return");

    co_return {};
}

Ichor::Task<void> Ichor::HiredisService::stop() {
    redisAsyncDisconnect(_redisContext);

    _pollTimer->stopTimer();

    INTERNAL_DEBUG("HiredisService::stop() co_return");

    co_return;
}

void Ichor::HiredisService::addDependencyInstance(ILogger &logger, IService&) {
    _logger = &logger;
}

void Ichor::HiredisService::removeDependencyInstance(ILogger &logger, IService&) {
    _logger = nullptr;
}

void Ichor::HiredisService::setPriority(uint64_t priority) {
    _priority.store(priority, std::memory_order_release);
}

uint64_t Ichor::HiredisService::getPriority() {
    return _priority.load(std::memory_order_acquire);
}

Ichor::AsyncGenerator<Ichor::RedisAuthReply> Ichor::HiredisService::auth(std::string_view user, std::string_view password) {
    IchorRedisReply evt{};
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "AUTH %b %b", user.data(), user.size(), password.data(), password.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return RedisAuthReply{};
    }

    co_return RedisAuthReply{true};
}

Ichor::AsyncGenerator<Ichor::RedisSetReply> Ichor::HiredisService::set(std::string_view key, std::string_view value) {
    IchorRedisReply evt{};
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "SET %b %b", key.data(), key.size(), value.data(), value.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return RedisSetReply{};
    }

    co_return RedisSetReply{true, evt.reply->str};
}

Ichor::AsyncGenerator<Ichor::RedisSetReply> Ichor::HiredisService::set(std::string_view key, std::string_view value, RedisSetOptions const &opts) {
    auto buf = _formatSet(key, value, opts);

    IchorRedisReply evt{};
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, buf.data());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return RedisSetReply{};
    }

    co_return RedisSetReply{true, evt.reply->str};
}

void Ichor::HiredisService::setAndForget(std::string_view key, std::string_view value) {
    auto ret = redisAsyncCommand(_redisContext, nullptr, nullptr, "SET %b %b", key.data(), key.size(), value.data(), value.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command");
    }
}

void Ichor::HiredisService::setAndForget(std::string_view key, std::string_view value, RedisSetOptions const &opts) {
    auto buf = _formatSet(key, value, opts);

    auto ret = redisAsyncCommand(_redisContext, nullptr, nullptr, buf.data());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command");
    }
}

Ichor::AsyncGenerator<Ichor::RedisGetReply> Ichor::HiredisService::get(std::string_view key) {
    IchorRedisReply evt{};
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "GET %b", key.data(), key.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return RedisGetReply{};
    }

    co_return RedisGetReply{evt.reply->str};
}

void Ichor::HiredisService::onRedisConnect(int status) {
    if(status != REDIS_OK) {
        ICHOR_LOG_ERROR(_logger, "connect error {}", _redisContext->err);
        _redisContext = nullptr;
    }
}

void Ichor::HiredisService::onRedisDisconnect(int status) {
    if(status == REDIS_OK) {
        _redisContext = nullptr;
        _disconnectEvt.set();
    } else {
        ICHOR_LOG_WARN(_logger, "Redis disconnected, attempting reconnect, reason: \"{}\"", _redisContext->errstr);
        _redisContext = nullptr;
        if(!connect()) {
            GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(getServiceId(), getServiceId());
        }
    }
}


tl::expected<void, Ichor::StartError> Ichor::HiredisService::connect() {
    auto addrIt = getProperties().find("Address");
    auto portIt = getProperties().find("Port");

    if(addrIt == getProperties().end()) [[unlikely]] {
        throw std::runtime_error("Missing address when starting HiredisService");
    }
    if(portIt == getProperties().end()) [[unlikely]] {
        throw std::runtime_error("Missing port when starting HiredisService");
    }

    redisOptions opts{};
    REDIS_OPTIONS_SET_TCP(&opts, Ichor::any_cast<std::string&>(addrIt->second).c_str(), Ichor::any_cast<uint16_t>(portIt->second));
    opts.options |= REDIS_OPT_REUSEADDR;
    opts.options |= REDIS_OPT_NOAUTOFREEREPLIES;
    _redisContext = redisAsyncConnectWithOptions(&opts);
    if (_redisContext->err) [[unlikely]] {
        ICHOR_LOG_ERROR(_logger, "redis error {}", _redisContext->errstr);
        // handle error
        redisAsyncFree(_redisContext);
        _redisContext = nullptr;
        return tl::unexpected(StartError::FAILED);
    }

    _redisContext->data = this; /* store application pointer for the callbacks */

    if(redisPollAttach(_redisContext)) [[unlikely]] {
        ICHOR_LOG_ERROR(_logger, "redis error attaching poll");
        redisAsyncFree(_redisContext);
        _redisContext = nullptr;
        return tl::unexpected(StartError::FAILED);
    }
    redisAsyncSetConnectCallback(_redisContext, _onRedisConnect);
    redisAsyncSetDisconnectCallback(_redisContext, _onRedisDisconnect);

    return {};
}

#endif
