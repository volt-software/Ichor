#include <ichor/DependencyManager.h>
#include <ichor/services/redis/HiredisService.h>
#include <ichor/ScopeGuard.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/poll.h>
#include <fmt/format.h>

using namespace std::literals;

#define FMT_INLINE_BUFFER_SIZE 1024

#define ICHOR_WAIT_IF_NOT_CONNECTED \
if(_redisContext == nullptr) { \
    co_await _disconnectEvt;        \
    ICHOR_LOG_TRACE(_logger, "post-await\n");                                \
    if(_redisContext == nullptr) [[unlikely]] { \
        ICHOR_LOG_TRACE(_logger, "rediscontext null\n");                            \
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
        std::string origCommand;
        redisReply *reply;
        AsyncManualResetEvent evt{};
    };

    // this function assumes it is being called from the correct ichor thread.
    // the poll timer should take care of that.
    static void _onRedisConnect(const struct redisAsyncContext *c, int status) {
        auto *svc = reinterpret_cast<Ichor::HiredisService *>(c->data);
        svc->onRedisConnect(status);
    }

    // this function assumes it is being called from the correct ichor thread.
    // the poll timer should take care of that.
    static void _onRedisDisconnect(const struct redisAsyncContext *c, int status) {
        auto *svc = reinterpret_cast<Ichor::HiredisService *>(c->data);
        svc->onRedisDisconnect(status);
    }

    static void _printReply(redisReply const * const r, char const * const indent) {
        switch(r->type) {
            case REDIS_REPLY_ERROR:
            case REDIS_REPLY_STRING:
            case REDIS_REPLY_DOUBLE:
                fmt::print("{}String-ish reply from redis {} \"{}\"\n", indent, r->dval, r->str);
                break;
            case REDIS_REPLY_BIGNUM:
                fmt::print("{}Bignum reply from redis \"{}\"\n", indent, r->str);
                break;
            case REDIS_REPLY_INTEGER:
                fmt::print("{}Integer reply from redis {}\n", indent, r->integer);
                break;
            case REDIS_REPLY_VERB:
                fmt::print("{}Verb reply from redis \"{}\" \"{}\"\n", indent, r->vtype, r->str);
                break;
            case REDIS_REPLY_STATUS:
                fmt::print("{}Status reply from redis {}\n", indent, r->str);
                break;
            case REDIS_REPLY_NIL:
                fmt::print("{}Nil reply from redis\n", indent);
                break;
            case REDIS_REPLY_ARRAY:
                fmt::print("{}Array reply from redis\n", indent);
                for(size_t i = 0; i < r->elements; i++) {
                    _printReply(r->element[i], "\t");
                }
                break;
        }
        fmt::print("{}Unknown reply from redis type {}\n", indent, r->type);
    }

    // this function assumes it is being called from the correct ichor thread.
    // the poll timer should take care of that.
    static void _onAsyncReply(redisAsyncContext *c, void *reply, void *privdata) {
        auto *ichorReply = static_cast<IchorRedisReply*>(privdata);
        auto *svc = static_cast<HiredisService*>(c->data);
        ichorReply->reply = static_cast<redisReply *>(reply);
        if(svc->getDebug()) {
            fmt::print("hiredis command \"{}\" got reply from redis type {}\n", ichorReply->origCommand, reply == nullptr ? -1 : ichorReply->reply->type);
        }
        if(reply != nullptr && svc->getDebug()) {
            _printReply(ichorReply->reply, "");
        }

        if(reply != nullptr && ichorReply->reply->type == REDIS_REPLY_ERROR) {
            fmt::print("hiredis command \"{}\" got error from redis: {}\n", ichorReply->origCommand, ichorReply->reply->str);
        }

        ichorReply->evt.set();
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
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<ITimerFactory>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IEventQueue>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::HiredisService::start() {
    if(auto propIt = getProperties().find("Address"); propIt == getProperties().end()) [[unlikely]] {
        throw std::runtime_error("Missing address when starting HiredisService");
    }
    if(auto propIt = getProperties().find("Port"); propIt == getProperties().end()) [[unlikely]] {
        throw std::runtime_error("Missing port when starting HiredisService");
    }

    if(auto propIt = getProperties().find("TimeoutMs"); propIt != getProperties().end()) {
        _timeoutMs = Ichor::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("PollIntervalMs"); propIt != getProperties().end()) {
        _pollIntervalMs = Ichor::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("TryConnectIntervalMs"); propIt != getProperties().end()) {
        _tryConnectIntervalMs = Ichor::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("Debug"); propIt != getProperties().end()) {
        _debug = Ichor::any_cast<bool>(propIt->second);
    }

    _timeWhenDisconnected = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    _timeoutTimer = &_timerFactory->createTimer();
    _timeoutTimer->setCallbackAsync([this]() -> AsyncGenerator<IchorBehaviour> {
        ICHOR_LOG_INFO(_logger, "Trying to (re)connect");

        if(_timeoutTimerRunning) {
            co_return {};
        }

        bool error{};
        _timeoutTimerRunning = true;
        ScopeGuard sg([this, &error = error]() {
            uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
            if(_redisContext != nullptr) {
                ICHOR_LOG_INFO(_logger, "Connected");
                _timeoutTimer->stopTimer({});
                _startEvt.set();
            } else if(error || now > _timeWhenDisconnected + _timeoutMs) {
                ICHOR_LOG_INFO(_logger, "Could not connect within {:L} ms", _timeoutMs);
                _timeoutTimer->stopTimer({});
                _startEvt.set();
                _queue->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, getServiceId());
            }
            _timeoutTimerRunning = false;
        });

        auto addrIt = getProperties().find("Address");
        auto portIt = getProperties().find("Port");
        auto &addr = Ichor::any_cast<std::string&>(addrIt->second);
        auto port = Ichor::any_cast<uint16_t>(portIt->second);

        if(!connect(addr, port)) {
            ICHOR_LOG_ERROR(_logger, "Couldn't setup hiredis");
            error = true;
            co_return {};
        }

        auto i = co_await info();
        if(!i) {
            if(i.error() != RedisError::DISCONNECTED) {
                ICHOR_LOG_ERROR(_logger, "Couldn't get info from redis");
                error = true;
            }
            co_return {};
        }

        auto versionStr = i.value().find("redis_version");

        if(versionStr == i.value().end()) {
            ICHOR_LOG_ERROR(_logger, "Couldn't get proper info from redis:");
            for(auto const &[k, v] : i.value()) {
                ICHOR_LOG_ERROR(_logger, "\t{} : {}", k, v);
            }
            error = true;
            co_return {};
        }

        _redisVersion = parseStringAsVersion(versionStr->second);

        if(!_redisVersion) {
            ICHOR_LOG_ERROR(_logger, "Couldn't parse version from redis: \"{}\"", versionStr->second);
            error = true;
            co_return {};
        }

        co_return {};
    });
    _timeoutTimer->setChronoInterval(std::chrono::milliseconds(_tryConnectIntervalMs));

    auto &pollTimer = _timerFactory->createTimer();
    pollTimer.setCallback([this]() {
        if(_redisContext != nullptr) {
            redisPollTick(_redisContext, 0);
        }
    });
    pollTimer.setChronoInterval(std::chrono::milliseconds(_pollIntervalMs));
    pollTimer.startTimer();
    _timeoutTimer->startTimer();

    co_await _startEvt;

    if(_redisContext == nullptr) {
        co_return tl::unexpected(StartError::FAILED);
    }

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
    evt.origCommand = fmt::format("AUTH {} password of size {}", user, password.size());
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "AUTH %b %b", user.data(), user.size(), password.data(), password.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command auth");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->type == REDIS_REPLY_STATUS && evt.reply->str == "QUEUED"sv) {
        _queuedResponseTypes.emplace_back(typeNameHash<RedisAuthReply>());
        co_return tl::unexpected(RedisError::QUEUED);
    }

    co_return RedisAuthReply{true};
}

Ichor::Task<tl::expected<Ichor::RedisSetReply, Ichor::RedisError>> Ichor::HiredisService::set(std::string_view key, std::string_view value) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("SET {} {}", key, value);
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "SET %b %b", key.data(), key.size(), value.data(), value.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command set");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->type == REDIS_REPLY_STATUS && evt.reply->str == "QUEUED"sv) {
        _queuedResponseTypes.emplace_back(typeNameHash<RedisSetReply>());
        co_return tl::unexpected(RedisError::QUEUED);
    }

    co_return RedisSetReply{true, evt.reply->str};
}

Ichor::Task<tl::expected<Ichor::RedisSetReply, Ichor::RedisError>> Ichor::HiredisService::set(std::string_view key, std::string_view value, RedisSetOptions const &opts) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    auto buf = _formatSet(key, value, opts);

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("{}", buf.data());
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, buf.data());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command set opts");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->type == REDIS_REPLY_STATUS && evt.reply->str == "QUEUED"sv) {
        _queuedResponseTypes.emplace_back(typeNameHash<RedisSetReply>());
        co_return tl::unexpected(RedisError::QUEUED);
    }

    co_return RedisSetReply{true, evt.reply->str};
}

Ichor::Task<tl::expected<Ichor::RedisGetReply, Ichor::RedisError>> Ichor::HiredisService::get(std::string_view key) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("GET {}", key);
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "GET %b", key.data(), key.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command get");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->type == REDIS_REPLY_STATUS && evt.reply->str == "QUEUED"sv) {
        _queuedResponseTypes.emplace_back(typeNameHash<RedisGetReply>());
        co_return tl::unexpected(RedisError::QUEUED);
    }

    if(evt.reply->type == REDIS_REPLY_NIL) {
        co_return RedisGetReply{};
    }

    co_return RedisGetReply{evt.reply->str};
}

Ichor::Task<tl::expected<Ichor::RedisGetReply, Ichor::RedisError>> Ichor::HiredisService::getdel(std::string_view key) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    if(*_redisVersion < Version{6, 2, 0}) {
        co_return tl::unexpected(RedisError::FUNCTION_NOT_AVAILABLE_IN_SERVER);
    }

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("GETDEL {}", key);
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "GETDEL %b", key.data(), key.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command getdel");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->type == REDIS_REPLY_STATUS && evt.reply->str == "QUEUED"sv) {
        _queuedResponseTypes.emplace_back(typeNameHash<RedisGetReply>());
        co_return tl::unexpected(RedisError::QUEUED);
    }

    co_return RedisGetReply{evt.reply->str};
}

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::del(std::string_view keys) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("DEL {}", keys);
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "DEL %b", keys.data(), keys.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command del");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->type == REDIS_REPLY_STATUS && evt.reply->str == "QUEUED"sv) {
        _queuedResponseTypes.emplace_back(typeNameHash<RedisIntegerReply>());
        co_return tl::unexpected(RedisError::QUEUED);
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::incr(std::string_view keys) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("INCR {}", keys);
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "INCR %b", keys.data(), keys.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command incr");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->type == REDIS_REPLY_STATUS && evt.reply->str == "QUEUED"sv) {
        _queuedResponseTypes.emplace_back(typeNameHash<RedisIntegerReply>());
        co_return tl::unexpected(RedisError::QUEUED);
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::incrBy(std::string_view keys, int64_t incr) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("INCRBY {} {}", keys, incr);
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "INCRBY %b %i", keys.data(), keys.size(), incr);
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command incrBy");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->type == REDIS_REPLY_STATUS && evt.reply->str == "QUEUED"sv) {
        _queuedResponseTypes.emplace_back(typeNameHash<RedisIntegerReply>());
        co_return tl::unexpected(RedisError::QUEUED);
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::incrByFloat(std::string_view keys, double incr) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    if(*_redisVersion < Version{2, 6, 0}) {
        co_return tl::unexpected(RedisError::FUNCTION_NOT_AVAILABLE_IN_SERVER);
    }

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("INCRBYFLOAT {} {}", keys, incr);
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "INCRBYFLOAT %b %f", keys.data(), keys.size(), incr);
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command incrByFloat");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->type == REDIS_REPLY_STATUS && evt.reply->str == "QUEUED"sv) {
        _queuedResponseTypes.emplace_back(typeNameHash<RedisIntegerReply>());
        co_return tl::unexpected(RedisError::QUEUED);
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::decr(std::string_view keys) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("DECR {}", keys);
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "DECR %b", keys.data(), keys.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command decr");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->type == REDIS_REPLY_STATUS && evt.reply->str == "QUEUED"sv) {
        _queuedResponseTypes.emplace_back(typeNameHash<RedisIntegerReply>());
        co_return tl::unexpected(RedisError::QUEUED);
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::decrBy(std::string_view keys, int64_t decr) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("DECRBY {} {}", keys, decr);
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "DECRBY %b %i", keys.data(), keys.size(), decr);
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command decrBy");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->type == REDIS_REPLY_STATUS && evt.reply->str == "QUEUED"sv) {
        _queuedResponseTypes.emplace_back(typeNameHash<RedisIntegerReply>());
        co_return tl::unexpected(RedisError::QUEUED);
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

Ichor::Task<tl::expected<Ichor::RedisIntegerReply, Ichor::RedisError>> Ichor::HiredisService::strlen(std::string_view key) {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    if(*_redisVersion < Version{2, 2, 0}) {
        co_return tl::unexpected(RedisError::FUNCTION_NOT_AVAILABLE_IN_SERVER);
    }

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("STRLEN {}", key);
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "STRLEN %b", key.data(), key.size());
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command strlen");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->type == REDIS_REPLY_STATUS && evt.reply->str == "QUEUED"sv) {
        _queuedResponseTypes.emplace_back(typeNameHash<RedisIntegerReply>());
        co_return tl::unexpected(RedisError::QUEUED);
    }

    co_return RedisIntegerReply{evt.reply->integer};
}

Ichor::Task<tl::expected<void, Ichor::RedisError>> Ichor::HiredisService::multi() {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    if(*_redisVersion < Version{1, 2, 0}) {
        co_return tl::unexpected(RedisError::FUNCTION_NOT_AVAILABLE_IN_SERVER);
    }

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("MULTI");
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "MULTI");
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command strlen");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }

    co_return {};
}

Ichor::Task<tl::expected<std::vector<std::variant<Ichor::RedisGetReply, Ichor::RedisSetReply, Ichor::RedisAuthReply, Ichor::RedisIntegerReply>>, Ichor::RedisError>> Ichor::HiredisService::exec() {
    ScopeGuard sg{[this]() {
        _queuedResponseTypes.clear();
    }};

    ICHOR_WAIT_IF_NOT_CONNECTED;

    if(*_redisVersion < Version{1, 2, 0}) {
        co_return tl::unexpected(RedisError::FUNCTION_NOT_AVAILABLE_IN_SERVER);
    }

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("EXEC");
    auto redisRet = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "EXEC");
    if(redisRet == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command strlen");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type != REDIS_REPLY_ARRAY) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }
    if(evt.reply->elements != _queuedResponseTypes.size()) {
        ICHOR_LOG_ERROR(_logger, "Please open a bug report, number of queued responses does not match");
        std::terminate();
    }

    std::vector<std::variant<Ichor::RedisGetReply, Ichor::RedisSetReply, Ichor::RedisAuthReply, Ichor::RedisIntegerReply>> ret;
    for(size_t i = 0; i < evt.reply->elements; i++) {
        auto *element = evt.reply->element[i];
        switch (_queuedResponseTypes[i]) {
            case typeNameHash<RedisGetReply>():
                if(element->type != REDIS_REPLY_STRING && element->type != REDIS_REPLY_NIL) [[unlikely]] {
                    ICHOR_LOG_ERROR(_logger, "Please open a bug report, queued responses does not match");
                    std::terminate();
                }
                if(element->type == REDIS_REPLY_STRING) {
                    ret.emplace_back(RedisGetReply{element->str});
                } else {
                    ret.emplace_back(RedisGetReply{});
                }
                break;
            case typeNameHash<RedisSetReply>():
                if(element->type != REDIS_REPLY_STATUS) [[unlikely]] {
                    ICHOR_LOG_ERROR(_logger, "Please open a bug report, queued responses does not match");
                    std::terminate();
                }
                ret.emplace_back(RedisSetReply{true, element->str});
                break;
            case typeNameHash<RedisAuthReply>():
                if(element->type != REDIS_REPLY_STRING) [[unlikely]] {
                    ICHOR_LOG_ERROR(_logger, "Please open a bug report, queued responses does not match");
                    std::terminate();
                }
                ret.emplace_back(RedisAuthReply{true});
                break;
            case typeNameHash<RedisIntegerReply>():
                if(element->type != REDIS_REPLY_INTEGER) [[unlikely]] {
                    ICHOR_LOG_ERROR(_logger, "Please open a bug report, queued responses does not match");
                    std::terminate();
                }
                ret.emplace_back(RedisIntegerReply{element->integer});
                break;
            default:
                ICHOR_LOG_ERROR(_logger, "Please open a bug report, queued responses does not match");
                std::terminate();
        }
    }

    co_return ret;
}

Ichor::Task<tl::expected<void, Ichor::RedisError>> Ichor::HiredisService::discard() {
    ScopeGuard sg{[this]() {
        _queuedResponseTypes.clear();
    }};

    ICHOR_WAIT_IF_NOT_CONNECTED;

    if(*_redisVersion < Version{2, 0, 0}) {
        co_return tl::unexpected(RedisError::FUNCTION_NOT_AVAILABLE_IN_SERVER);
    }

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("DISCARD");
    auto ret = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "DISCARD");
    if(ret == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command strlen");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }

    co_return {};
}

Ichor::Task<tl::expected<std::unordered_map<std::string, std::string>, Ichor::RedisError>> Ichor::HiredisService::info() {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    IchorRedisReply evt{};
    evt.origCommand = fmt::format("INFO");
    auto commandRet = redisAsyncCommand(_redisContext, _onAsyncReply, &evt, "INFO");
    if(commandRet == REDIS_ERR) [[unlikely]] {
        throw std::runtime_error("couldn't run async command info");
    }
    co_await evt.evt;

    if(evt.reply == nullptr) [[unlikely]] {
        co_return tl::unexpected(RedisError::DISCONNECTED);
    }
    if(evt.reply->type == REDIS_REPLY_ERROR) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }

    std::string_view infoView{evt.reply->str};
    auto infoSplit = split(infoView, "\r\n", false);
    std::unordered_map<std::string, std::string> ret;
    ret.reserve(infoSplit.size());
    for(auto const &i : infoSplit) {
        if(i.find(":") != std::string_view::npos) {
            auto lineSplit = split(i, ":", false);
            if(lineSplit.size() != 2) {
                continue;
            }
            ret.emplace(lineSplit[0], lineSplit[1]);
        }
    }

    co_return ret;
}
Ichor::Task<tl::expected<Ichor::Version, Ichor::RedisError>> Ichor::HiredisService::getServerVersion() {
    ICHOR_WAIT_IF_NOT_CONNECTED;

    if(!_redisVersion) {
        co_return tl::unexpected(RedisError::UNKNOWN);
    }

    co_return _redisVersion.value();
}

void Ichor::HiredisService::onRedisConnect(int status) {
    if(status != REDIS_OK) {
        if(_timeoutTimer->getState() == TimerState::STOPPED) {
            if(_redisContext != nullptr) {
                ICHOR_LOG_ERROR(_logger, "Couldn't connect because {} \"{}\"", _redisContext->err, _redisContext->errstr);
            } else {
                ICHOR_LOG_ERROR(_logger, "Couldn't connect reason unknown (allocation error?)");
            }
            _redisContext = nullptr;
            _timeWhenDisconnected = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
            _timeoutTimer->startTimer();
        } else {
            _redisContext = nullptr;
        }
    } else {
        _timeoutTimer->stopTimer({});
        _disconnectEvt.set();
    }
}

void Ichor::HiredisService::onRedisDisconnect(int status) {
    if(status == REDIS_OK) {
        // user requested disconnect
        ICHOR_LOG_INFO(_logger, "Disconnected");
        _redisContext = nullptr;
    } else {
        if(_redisContext != nullptr) {
            ICHOR_LOG_ERROR(_logger, "Disconnected because {} \"{}\"", _redisContext->err, _redisContext->errstr);
        } else {
            ICHOR_LOG_ERROR(_logger, "Disconnected");
        }
        if(_timeoutTimer->getState() == TimerState::STOPPED) {
            _disconnectEvt.reset();
            _redisContext = nullptr;
            _timeWhenDisconnected = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
            _timeoutTimer->startTimer();
        } else {
            _redisContext = nullptr;
        }
    }
}

void Ichor::HiredisService::setDebug(bool debug) noexcept {
    _debug = debug;
}

bool Ichor::HiredisService::getDebug() const noexcept {
    return _debug;
}


tl::expected<void, Ichor::StartError> Ichor::HiredisService::connect(std::string const &addr, uint16_t port) {
    redisOptions opts{};
    REDIS_OPTIONS_SET_TCP(&opts, addr.c_str(), port);
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
