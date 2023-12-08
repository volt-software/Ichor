#include <ichor/DependencyManager.h>
#include <ichor/services/redis/HiredisService.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/poll.h>
#include <ichor/events/RunFunctionEvent.h>

#define FMT_INLINE_BUFFER_SIZE 1024

#define ICHOR_WAIT_IF_NOT_CONNECTED \
if(_redisContext == nullptr) { \
    co_await _disconnectEvt;        \
    fmt::print("post-await\n");                                \
    if(_redisContext == nullptr) [[unlikely]] { \
        fmt::print("rediscontext null\n");                            \
        co_return tl::unexpected(Ichor::RedisError::DISCONNECTED); \
    } \
}                                   \
static_assert(true)

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

        GetThreadLocalManager().getEventQueue().pushEvent<RunFunctionEvent>(svc->getServiceId(), [ichorReply]() {
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
    reg.registerDependency<ITimerFactory>(this, true);
    reg.registerDependency<IEventQueue>(this, true);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::HiredisService::start() {
    if(getProperties().contains("PollIntervalMs")) {
        _pollIntervalMs = Ichor::any_cast<uint64_t>(getProperties()["PollIntervalMs"]);
    }
    if(getProperties().contains("TimeoutMs")) {
        _timeoutMs = Ichor::any_cast<uint64_t>(getProperties()["TimeoutMs"]);
    }
    if(getProperties().contains("TryConnectIntervalMs")) {
        _tryConnectIntervalMs = Ichor::any_cast<uint64_t>(getProperties()["TryConnectIntervalMs"]);
    }

    auto outcome = connect();
    if(!outcome) {
        co_return tl::unexpected(StartError::FAILED);
    }

    auto &pollTimer = _timerFactory->createTimer();
    pollTimer.setCallback([this]() {
        if(_redisContext != nullptr) {
            redisPollTick(_redisContext, 0);
        }
    });
    pollTimer.setChronoInterval(std::chrono::milliseconds(_pollIntervalMs));
    pollTimer.startTimer();

    _timeoutTimer = &_timerFactory->createTimer();
    _timeoutTimer->setCallback([this]() {
        fmt::print("Trying to reconnect\n");
        if(!connect()) {
            _queue->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, getServiceId());
            return;
        }

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        if(now > _timeWhenDisconnected + _timeoutMs) {
            _disconnectEvt.set();
            _queue->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, getServiceId());
            _timeoutTimer->stopTimer();
        }
    });
    _timeoutTimer->setChronoInterval(std::chrono::milliseconds(_tryConnectIntervalMs));

    INTERNAL_DEBUG("HiredisService::start() co_return");

    co_return {};
}

Ichor::Task<void> Ichor::HiredisService::stop() {
    redisAsyncDisconnect(_redisContext);

    INTERNAL_DEBUG("HiredisService::stop() co_return");

    co_return;
}

void Ichor::HiredisService::addDependencyInstance(ILogger &logger, IService&) {
    _logger = &logger;
}

void Ichor::HiredisService::removeDependencyInstance(ILogger &, IService&) {
    _logger = nullptr;
}

void Ichor::HiredisService::addDependencyInstance(ITimerFactory &factory, IService&) {
    _timerFactory = &factory;
}

void Ichor::HiredisService::removeDependencyInstance(ITimerFactory &, IService&) {
    _timerFactory = nullptr;
}

void Ichor::HiredisService::addDependencyInstance(IEventQueue &queue, IService&) {
    _queue = &queue;
}

void Ichor::HiredisService::removeDependencyInstance(IEventQueue &, IService&) {
    _queue = nullptr;
}

Ichor::Task<tl::expected<Ichor::RedisAuthReply, Ichor::RedisError>> Ichor::HiredisService::auth(std::string_view user, std::string_view password) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

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

Ichor::Task<tl::expected<Ichor::RedisSetReply, Ichor::RedisError>> Ichor::HiredisService::set(std::string_view key, std::string_view value) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

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

Ichor::Task<tl::expected<Ichor::RedisSetReply, Ichor::RedisError>> Ichor::HiredisService::set(std::string_view key, std::string_view value, RedisSetOptions const &opts) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

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

Ichor::Task<tl::expected<Ichor::RedisGetReply, Ichor::RedisError>> Ichor::HiredisService::get(std::string_view key) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

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

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::del(std::string_view keys) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "DEL %b", keys.data(), keys.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return RedisIntegerReply{};
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::incr(std::string_view keys) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "INCR %b", keys.data(), keys.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return RedisIntegerReply{};
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::incrBy(std::string_view keys, int64_t incr) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "INCRBY %b %i", keys.data(), keys.size(), incr);
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return RedisIntegerReply{};
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::incrByFloat(std::string_view keys, double incr) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "INCRBYFLOAT %b %f", keys.data(), keys.size(), incr);
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return RedisIntegerReply{};
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::decr(std::string_view keys) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "DECR %b", keys.data(), keys.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return RedisIntegerReply{};
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::decrBy(std::string_view keys, int64_t decr) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "DECRBY %b %i", keys.data(), keys.size(), decr);
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return RedisIntegerReply{};
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

void Ichor::HiredisService::onRedisConnect(int status) {
    if(status != REDIS_OK) {
        if(!_timeoutTimer->running()) {
            ICHOR_LOG_ERROR(_logger, "connect error {}", _redisContext->err);
            _redisContext = nullptr;
            _timeWhenDisconnected = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
            _timeoutTimer->startTimer();
        } else {
            _redisContext = nullptr;
        }
    } else {
        _timeoutTimer->stopTimer();
        _disconnectEvt.set();
    }
}

void Ichor::HiredisService::onRedisDisconnect(int status) {
    if(status == REDIS_OK) {
        _redisContext = nullptr;
    } else {
        if(!_timeoutTimer->running()) {
            _redisContext = nullptr;
            ICHOR_LOG_ERROR(_logger, "connect error {}", _redisContext->err);
            _timeWhenDisconnected = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
            _timeoutTimer->startTimer();
        } else {
            _redisContext = nullptr;
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
