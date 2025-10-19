#include <iterator>
#include <fmt/base.h>

#include <ichor/services/etcd/EtcdV2Service.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ScopeGuard.h>
#include <ichor/stl/StringUtils.h>
#include <base64/base64.h>
#include <ichor/glaze.h>
#include <glaze/util/type_traits.hpp>
#include <ichor/ServiceExecutionScope.h>

using namespace Ichor::Etcdv2::v1;
using namespace Ichor::v1;

template <>
struct glz::meta<EtcdReplyNode> {
    using T = EtcdReplyNode;
    static constexpr auto value = object(
        "createdIndex", &T::createdIndex,
        "modifiedIndex", &T::modifiedIndex,
        "key", &T::key,
        "value", &T::value,
        "ttl", &T::ttl,
        "expiration", &T::expiration,
        "dir", &T::dir,
        "nodes", &T::nodes
    );
};

template <>
struct glz::meta<EtcdReply> {
    using T = EtcdReply;
    static constexpr auto value = object(
        "action", &T::action,
        "node", &T::node,
        "prevNode", &T::prevNode,
        "index", &T::index,
        "cause", &T::cause,
        "errorCode", &T::errorCode,
        "message", &T::message
    );
};

template <>
struct glz::meta<EtcdLeaderInfoStats> {
    using T = EtcdLeaderInfoStats;
    static constexpr auto value = object(
        "leader", &T::leader,
        "uptime", &T::uptime,
        "startTime", &T::startTime
    );
};

template <>
struct glz::meta<EtcdSelfStats> {
    using T = EtcdSelfStats;
    static constexpr auto value = object(
        "name", &T::name,
        "id", &T::id,
        "state", &T::state,
        "startTime", &T::startTime,
        "leaderInfo", &T::leaderInfo,
        "recvAppendRequestCnt", &T::recvAppendRequestCnt,
        "sendAppendRequestCnt", &T::sendAppendRequestCnt
    );
};

template <>
struct glz::meta<EtcdStoreStats> {
    using T = EtcdStoreStats;
    static constexpr auto value = object(
        "compareAndSwapFail", &T::compareAndSwapFail,
        "compareAndSwapSuccess", &T::compareAndSwapSuccess,
        "compareAndDeleteSuccess", &T::compareAndDeleteSuccess,
        "compareAndDeleteFail", &T::compareAndDeleteFail,
        "createFail", &T::createFail,
        "createSuccess", &T::createSuccess,
        "deleteFail", &T::deleteFail,
        "deleteSuccess", &T::deleteSuccess,
        "expireCount", &T::expireCount,
        "getsFail", &T::getsFail,
        "getsSuccess", &T::getsSuccess,
        "setsFail", &T::setsFail,
        "setsSuccess", &T::setsSuccess,
        "updateFail", &T::updateFail,
        "updateSuccess", &T::updateSuccess,
        "watchers", &T::watchers
    );
};

namespace Ichor::Etcdv2::v1 {
    struct EtcdHealthReply final {
        std::string health;
        std::optional<std::string> reason;
    };
    struct EtcdEnableAuthReply final {
        bool enabled;
    };
}


template <>
struct glz::meta<EtcdHealthReply> {
    using T = EtcdHealthReply;
    static constexpr auto value = object(
        "health", &T::health,
        "reason", &T::reason
    );
};

template <>
struct glz::meta<EtcdEnableAuthReply> {
    using T = EtcdEnableAuthReply;
    static constexpr auto value = object(
        "enabled", &T::enabled
    );
};

template <>
struct glz::meta<EtcdVersionReply> {
    using T = EtcdVersionReply;
    static constexpr auto value = object(
        "etcdserver", &T::etcdserver,
        "etcdcluster", &T::etcdcluster
    );
};

template <>
struct glz::meta<EtcdKvReply> {
    using T = EtcdKvReply;
    static constexpr auto value = object(
        "read", &T::read,
        "write", &T::write
    );
};

template <>
struct glz::meta<EtcdPermissionsReply> {
    using T = EtcdPermissionsReply;
    static constexpr auto value = object(
        "kv", &T::kv
    );
};

template <>
struct glz::meta<EtcdRoleReply> {
    using T = EtcdRoleReply;
    static constexpr auto value = object(
        "role", &T::role,
        "permissions", &T::permissions,
        "grant", &T::grant,
        "revoke", &T::revoke
    );
};

template <>
struct glz::meta<EtcdRolesReply> {
    using T = EtcdRolesReply;
    static constexpr auto value = object(
        "roles", &T::roles
    );
};

template <>
struct glz::meta<EtcdUserReply> {
    using T = EtcdUserReply;
    static constexpr auto value = object(
        "user", &T::user,
        "password", &T::password,
        "roles", &T::roles,
        "grant", &T::grant,
        "revoke", &T::revoke
    );
};

template <>
struct glz::meta<EtcdUpdateUserReply> {
    using T = EtcdUpdateUserReply;
    static constexpr auto value = object(
        "user", &T::user,
        "roles", &T::roles
    );
};

template <>
struct glz::meta<EtcdUsersReply> {
    using T = EtcdUsersReply;
    static constexpr auto value = object(
        "users", &T::users
    );
};

EtcdService::EtcdService(DependencyRegister &reg, Properties props) : AdvancedService<EtcdService>(std::move(props)) {
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED, getProperties());
    reg.registerDependency<IHttpConnectionService>(this, DependencyFlags::REQUIRED | DependencyFlags::ALLOW_MULTIPLE, getProperties());
    reg.registerDependency<IClientFactory<IHttpConnectionService>>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> EtcdService::start() {
    auto addrIt = getProperties().find("Address");
    auto portIt = getProperties().find("Port");

    if(addrIt == getProperties().end()) [[unlikely]] {
        ICHOR_LOG_ERROR(_logger, "Missing address");
        co_return tl::unexpected(StartError::FAILED);
    }
    if(portIt == getProperties().end()) [[unlikely]] {
        ICHOR_LOG_ERROR(_logger, "Missing port");
        co_return tl::unexpected(StartError::FAILED);
    }
    ICHOR_LOG_TRACE(_logger, "Started");
    co_return {};
}

Ichor::Task<void> EtcdService::stop() {
    ICHOR_LOG_TRACE(_logger, "Stopped");
    co_return;
}

void EtcdService::addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &) {
    _logger = std::move(logger);
    ICHOR_LOG_TRACE(_logger, "Added logger");
}

void EtcdService::removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*>, IService&) {
    ICHOR_LOG_TRACE(_logger, "Removed logger");
    _logger = nullptr;
}

void EtcdService::addDependencyInstance(Ichor::ScopedServiceProxy<IHttpConnectionService*> conn, IService &) {
    if(_mainConn == nullptr) {
        ICHOR_LOG_TRACE(_logger, "Added MainCon");
        _mainConn = std::move(conn);
        return;
    }

    // In a condition where we're stopping, e.g. the mainConn is already gone, the existing requests may still get 'added', but we've already cleaned up all requests
    if(_connRequests.empty()) {
        return;
    }

    ICHOR_LOG_TRACE(_logger, "Added conn, triggering coroutine");

    _connRequests.top().conn = std::move(conn);
    _connRequests.top().event.set();
}

void EtcdService::removeDependencyInstance(Ichor::ScopedServiceProxy<IHttpConnectionService*> conn, IService &) {
    if(_mainConn == conn) {
        ICHOR_LOG_TRACE(_logger, "Removing MainCon");
        _mainConn = nullptr;
    }

    ICHOR_LOG_TRACE(_logger, "Removing conn {}", getServiceState());
    if(getServiceState() == ServiceState::STOPPING || getServiceState() == ServiceState::UNINJECTING) {
        while(!_connRequests.empty()) {
            _connRequests.top().event.set();
            _connRequests.pop();
        }
    }
}

void EtcdService::addDependencyInstance(Ichor::ScopedServiceProxy<IClientFactory<IHttpConnectionService>*> factory, IService &) {
    ICHOR_LOG_TRACE(_logger, "Added clientFactory");
    _clientFactory = std::move(factory);
}

void EtcdService::removeDependencyInstance(Ichor::ScopedServiceProxy<IClientFactory<IHttpConnectionService>*>, IService&) {
    ICHOR_LOG_TRACE(_logger, "Removed clientFactory");
    _clientFactory = nullptr;
}

Ichor::Task<tl::expected<EtcdReply, EtcdError>> EtcdService::put(std::string_view key, std::string_view value, tl::optional<std::string_view> previous_value, tl::optional<uint64_t> previous_index, tl::optional<bool> previous_exists, tl::optional<uint64_t> ttl_second, bool refresh, bool dir, bool in_order) {
    std::vector<uint8_t> msg_buf;
    fmt::format_to(FmtU8Inserter(msg_buf), "value={}", value);
    if(ttl_second) {
        fmt::format_to(FmtU8Inserter(msg_buf), "&ttl={}", *ttl_second);
    }
    if(refresh) {
        fmt::format_to(FmtU8Inserter(msg_buf), "&refresh=true");
    }
    if(previous_value) {
        fmt::format_to(FmtU8Inserter(msg_buf), "&prevValue={}", *previous_value);
    }
    if(previous_index) {
        fmt::format_to(FmtU8Inserter(msg_buf), "&prevIndex={}", *previous_index);
    }
    if(previous_exists) {
        fmt::format_to(FmtU8Inserter(msg_buf), "&prevExist={}", (*previous_exists) ? "true" : "false");
    }
    if(dir) {
        fmt::format_to(FmtU8Inserter(msg_buf), "&dir=true");
    }
    ICHOR_LOG_TRACE(_logger, "put body {}", std::string_view{(char*)msg_buf.data(), msg_buf.size()});
    unordered_map<std::string, std::string> headers{{"Content-Type", "application/x-www-form-urlencoded"}};

    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }
    HttpMethod method = in_order ? HttpMethod::post : HttpMethod::put;
    auto http_reply = co_await _mainConn->sendAsync(method, fmt::format("/v2/keys/{}", key), std::move(headers), std::move(msg_buf));

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http send error {}", fmt::format("/v2/keys/{}", key), http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "put status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok && http_reply->status != HttpStatus::created && http_reply->status != HttpStatus::forbidden && http_reply->status != HttpStatus::precondition_failed && http_reply->status != HttpStatus::not_found) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", fmt::format("/v2/keys/{}", key), (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    auto cluster_id = http_reply->headers.find("X-Etcd-Cluster-Id");
    auto etcd_index = http_reply->headers.find("X-Etcd-Index");
    auto raft_index = http_reply->headers.find("X-Raft-Index");
    auto raft_term = http_reply->headers.find("X-Raft-Term");

    if(cluster_id != end(http_reply->headers)) {
        etcd_reply.x_etcd_cluster_id = cluster_id->second;
    }
    if(etcd_index != end(http_reply->headers)) {
        etcd_reply.x_etcd_index = Ichor::v1::FastAtoiu(etcd_index->second.c_str());
    }
    if(raft_index != end(http_reply->headers)) {
        etcd_reply.x_raft_index = Ichor::v1::FastAtoiu(raft_index->second.c_str());
    }
    if(raft_term != end(http_reply->headers)) {
        etcd_reply.x_raft_term = Ichor::v1::FastAtoiu(raft_term->second.c_str());
    }

    co_return etcd_reply;
}

ICHOR_CORO_WRAPPER Ichor::Task<tl::expected<EtcdReply, EtcdError>> EtcdService::put(std::string_view key, std::string_view value, tl::optional<uint64_t> ttl_second, bool refresh) {
    return put(key, value, {}, {}, {}, ttl_second, refresh, false, false);
}

ICHOR_CORO_WRAPPER Ichor::Task<tl::expected<EtcdReply, EtcdError>> EtcdService::put(std::string_view key, std::string_view value, tl::optional<uint64_t> ttl_second) {
    return put(key, value, {}, {}, {}, ttl_second, false, false, false);
}

ICHOR_CORO_WRAPPER Ichor::Task<tl::expected<EtcdReply, EtcdError>> EtcdService::put(std::string_view key, std::string_view value) {
    return put(key, value, {}, {}, {}, {}, false, false, false);
}

Ichor::Task<tl::expected<EtcdReply, EtcdError>> EtcdService::get(std::string_view key, bool recursive, bool sorted, bool watch, tl::optional<uint64_t> watchIndex) {
    std::string url{fmt::format("/v2/keys/{}?", key)};

    // using watches blocks the connection and thus blocks every other call
    // This implementation requests a new HttpConnection specifically for this and cleans it up afterwards
    Ichor::ScopedServiceProxy<IHttpConnectionService*> connToUse = _mainConn;
    tl::optional<uint64_t> connIdToClean{};
    ScopeGuard sg{[this, &connIdToClean]() {
        if(connIdToClean && _clientFactory != nullptr) {
            _clientFactory->removeConnection(this, *connIdToClean);
        }
    }};

    if(recursive) {
        fmt::format_to(std::back_inserter(url), "recursive=true&");
    }
    if(sorted) {
        fmt::format_to(std::back_inserter(url), "sorted=true&");
    }
    if(watch) {
        fmt::format_to(std::back_inserter(url), "wait=true&");
    }
    if(watchIndex) {
        fmt::format_to(std::back_inserter(url), "waitIndex={}&", *watchIndex);
    }

    ICHOR_LOG_TRACE(_logger, "get url {}", url);

    // can't combine with above, as that would mean not tracing the request before a potential co_return
    if(watch) {
        ConnRequest &request = _connRequests.emplace();
        connIdToClean = _clientFactory->createNewConnection(this, getProperties());
        co_await request.event;
        if(request.conn == nullptr) {
            co_return tl::unexpected(EtcdError::QUITTING);
        }
        connToUse = request.conn;
    }

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await connToUse->sendAsync(HttpMethod::get, url, std::move(headers), {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http send error {}", fmt::format("/v2/keys/{}", key), http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "get status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok && http_reply->status != HttpStatus::not_found) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    if(http_reply->body.empty() && http_reply->status == HttpStatus::ok) {
        co_return tl::unexpected(EtcdError::CONNECTION_CLOSED_PREMATURELY_TRY_AGAIN);
    }

    EtcdReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

ICHOR_CORO_WRAPPER Ichor::Task<tl::expected<EtcdReply, EtcdError>> EtcdService::get(std::string_view key) {
    return get(key, false, false, false, {});
}

Ichor::Task<tl::expected<EtcdReply, EtcdError>> EtcdService::del(std::string_view key, bool recursive, bool dir) {
    std::string url = fmt::format("/v2/keys/{}?", key);
    if(dir) {
        fmt::format_to(std::back_inserter(url), "dir=true&");
    }
    if(recursive) {
        fmt::format_to(std::back_inserter(url), "recursive=true&");
    }

    ICHOR_LOG_TRACE(_logger, "del url {}", url);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::delete_, url, std::move(headers), {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http send error {}", fmt::format("/v2/keys/{}", key), http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "del status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdReply, EtcdError>> EtcdService::leaderStatistics() {
    std::string url = fmt::format("/v2/stats/leader");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "leaderStatistics http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_ERROR(_logger, "leaderStatistics json {}", http_reply->body.empty() ? "" : http_reply->body.empty() ? "" : (char*)http_reply->body.data());
    co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
}

Ichor::Task<tl::expected<EtcdSelfStats, EtcdError>> EtcdService::selfStatistics() {
    std::string url = fmt::format("/v2/stats/self");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "selfStatistics http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "selfStatistics status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdSelfStats etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdStoreStats, EtcdError>> EtcdService::storeStatistics() {
    std::string url = fmt::format("/v2/stats/store");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "storeStatistics http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "storeStatistics status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdStoreStats etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdVersionReply, EtcdError>> EtcdService::version() {
    std::string url = fmt::format("/version");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "version http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "version status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdVersionReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<bool, EtcdError>> EtcdService::health() {
    std::string url = fmt::format("/health");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "health http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "health status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdHealthReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply.health == "true";
}

Ichor::Task<tl::expected<bool, EtcdError>> EtcdService::authStatus() {
    std::string url = fmt::format("/v2/auth/enable");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "authSTatus http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "authStatus status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdEnableAuthReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply.enabled;
}

Ichor::Task<tl::expected<bool, EtcdError>> EtcdService::enableAuth() {
    std::string url = fmt::format("/v2/auth/enable");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, {}, {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "enableAuth http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "enableAuth status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status == HttpStatus::bad_request) {
        co_return tl::unexpected(EtcdError::ROOT_USER_NOT_YET_CREATED);
    }

    if(http_reply->status != HttpStatus::ok && http_reply->status != HttpStatus::conflict) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    co_return true;
}

Ichor::Task<tl::expected<bool, EtcdError>> EtcdService::disableAuth() {
    std::string url = fmt::format("/v2/auth/enable");

    if(!_auth) {
        co_return tl::unexpected(EtcdError::NO_AUTHENTICATION_SET);
    }

    // forced to declare as a variable due to internal compiler errors in gcc <= 12.3.0
    unordered_map<std::string, std::string> headers{{"Authorization", *_auth}};
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::delete_, url, std::move(headers), {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "disableAuth http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "disableAuth status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status == HttpStatus::unauthorized) {
        co_return tl::unexpected(EtcdError::UNAUTHORIZED);
    }

    if(http_reply->status != HttpStatus::ok && http_reply->status != HttpStatus::conflict) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    co_return true;
}

Ichor::Task<tl::expected<EtcdUsersReply, EtcdError>> EtcdService::getUsers() {
    std::string url = fmt::format("/v2/auth/users");

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, std::move(headers), {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "getUsers http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "getUsers status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdUsersReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdUserReply, EtcdError>> EtcdService::getUserDetails(std::string_view user) {
    std::string url = fmt::format("/v2/auth/users/{}", user);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, std::move(headers), {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "getUserDetails http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "getUserDetails status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdUserReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

void EtcdService::setAuthentication(std::string_view user, std::string_view pass) {
    _auth = fmt::format("{}:{}", user, pass);
    _auth = base64_encode(reinterpret_cast<unsigned const char*>(_auth->c_str()), _auth->length());
    _auth = fmt::format("Basic {}", *_auth);
}

void EtcdService::clearAuthentication() {
    _auth.reset();
}

tl::optional<std::string> EtcdService::getAuthenticationUser() const {
    if(!_auth) {
        return {};
    }

    auto full_str = base64_decode((*_auth).substr(6));
    auto pos = full_str.find(':');
    return full_str.substr(0, pos);
}

Ichor::Task<tl::expected<EtcdUpdateUserReply, EtcdError>> EtcdService::createUser(std::string_view user, std::string_view pass) {
    std::string url = fmt::format("/v2/auth/users/{}", user);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    EtcdUserReply user_create{};
    user_create.user = user;
    user_create.password = pass;

    std::vector<uint8_t> buf;
    auto err = glz::write_json(user_create, buf);
    if(err) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, couldn't serialize {}", url, glz::nameof(err.ec));
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "createUser http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "createUser status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok && http_reply->status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdUpdateUserReply etcd_reply;
    err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdUpdateUserReply, EtcdError>> EtcdService::grantUserRoles(std::string_view user, std::vector<std::string> roles) {
    std::string url = fmt::format("/v2/auth/users/{}", user);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    EtcdUserReply user_create{};
    user_create.user = user;
    user_create.grant = std::move(roles);

    std::vector<uint8_t> buf;
    auto err = glz::write_json(user_create, buf);
    if(err) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, couldn't serialize {}", url, glz::nameof(err.ec));
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "grantUserRoles http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "grantUserRoles status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok && http_reply->status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdUpdateUserReply etcd_reply;
    err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdUpdateUserReply, EtcdError>> EtcdService::revokeUserRoles(std::string_view user, std::vector<std::string> roles) {
    std::string url = fmt::format("/v2/auth/users/{}", user);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    EtcdUserReply user_create{};
    user_create.user = user;
    user_create.revoke = std::move(roles);

    std::vector<uint8_t> buf;
    auto err = glz::write_json(user_create, buf);
    if(err) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, couldn't serialize {}", url, glz::nameof(err.ec));
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "revokeUserRoles http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "revokeUserRoles status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok && http_reply->status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdUpdateUserReply etcd_reply;
    err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdUpdateUserReply, EtcdError>> EtcdService::updateUserPassword(std::string_view user, std::string_view pass) {
    std::string url = fmt::format("/v2/auth/users/{}", user);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    EtcdUserReply user_create{};
    user_create.user = user;
    user_create.password = pass;

    std::vector<uint8_t> buf;
    auto err = glz::write_json(user_create, buf);
    if(err) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, couldn't serialize {}", url, glz::nameof(err.ec));
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "updateUserPassword http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "updateUserPassword status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status != HttpStatus::ok && http_reply->status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdUpdateUserReply etcd_reply;
    err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<void, EtcdError>> EtcdService::deleteUser(std::string_view user) {
    std::string url = fmt::format("/v2/auth/users/{}", user);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::delete_, url, std::move(headers), {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "deleteUser http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "deleteUser status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status == HttpStatus::unauthorized) {
        co_return tl::unexpected(EtcdError::UNAUTHORIZED);
    }
    if(http_reply->status == HttpStatus::forbidden) {
        co_return tl::unexpected(EtcdError::REMOVING_ROOT_NOT_ALLOWED_WITH_AUTH_ENABLED);
    }
    if(http_reply->status == HttpStatus::not_found) {
        co_return tl::unexpected(EtcdError::NOT_FOUND);
    }

    if(http_reply->status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    co_return {};
}

Ichor::Task<tl::expected<EtcdRolesReply, EtcdError>> EtcdService::getRoles() {
    std::string url = fmt::format("/v2/auth/roles");

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, std::move(headers), {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "getRoles http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "getRoles status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status == HttpStatus::unauthorized) {
        co_return tl::unexpected(EtcdError::UNAUTHORIZED);
    }

    if(http_reply->status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdRolesReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdRoleReply, EtcdError>> EtcdService::getRole(std::string_view role) {
    std::string url = fmt::format("/v2/auth/roles/{}", role);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, std::move(headers), {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "getRole http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "getRole status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status == HttpStatus::unauthorized) {
        co_return tl::unexpected(EtcdError::UNAUTHORIZED);
    }
    if(http_reply->status == HttpStatus::not_found) {
        co_return tl::unexpected(EtcdError::NOT_FOUND);
    }

    if(http_reply->status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdRoleReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdRoleReply, EtcdError>> EtcdService::createRole(std::string_view role, std::vector<std::string> read_permissions, std::vector<std::string> write_permissions) {
    std::string url = fmt::format("/v2/auth/roles/{}", role);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    EtcdRoleReply role_create{};
    role_create.role = role;
    role_create.permissions.kv.read = std::move(read_permissions);
    role_create.permissions.kv.write = std::move(write_permissions);

    std::vector<uint8_t> buf;
    auto err = glz::write_json(role_create, buf);
    if(err) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, couldn't serialize {}", url, glz::nameof(err.ec));
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "createRole http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "createRole status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status == HttpStatus::unauthorized) {
        co_return tl::unexpected(EtcdError::UNAUTHORIZED);
    }
    if(http_reply->status == HttpStatus::not_found) {
        co_return tl::unexpected(EtcdError::NOT_FOUND);
    }
    if(http_reply->status == HttpStatus::conflict) {
        co_return tl::unexpected(EtcdError::DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT);
    }

    if(http_reply->status != HttpStatus::ok && http_reply->status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdRoleReply etcd_reply;
    err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdRoleReply, EtcdError>> EtcdService::grantRolePermissions(std::string_view role, std::vector<std::string> read_permissions, std::vector<std::string> write_permissions) {
    std::string url = fmt::format("/v2/auth/roles/{}", role);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    EtcdRoleReply role_create{};
    role_create.role = role;
    EtcdPermissionsReply granted{};
    granted.kv.read = std::move(read_permissions);
    granted.kv.write = std::move(write_permissions);
    role_create.grant = std::move(granted);

    std::vector<uint8_t> buf;
    auto err = glz::write_json(role_create, buf);
    if(err) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, couldn't serialize {}", url, glz::nameof(err.ec));
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "grantRolePermissions http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "grantRolePermissions status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status == HttpStatus::unauthorized) {
        co_return tl::unexpected(EtcdError::UNAUTHORIZED);
    }
    if(http_reply->status == HttpStatus::not_found) {
        co_return tl::unexpected(EtcdError::NOT_FOUND);
    }
    if(http_reply->status == HttpStatus::conflict) {
        co_return tl::unexpected(EtcdError::DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT);
    }

    if(http_reply->status != HttpStatus::ok && http_reply->status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdRoleReply etcd_reply;
    err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdRoleReply, EtcdError>> EtcdService::revokeRolePermissions(std::string_view role, std::vector<std::string> read_permissions, std::vector<std::string> write_permissions) {
    std::string url = fmt::format("/v2/auth/roles/{}", role);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    EtcdRoleReply role_create{};
    role_create.role = role;
    EtcdPermissionsReply revoked{};
    revoked.kv.read = std::move(read_permissions);
    revoked.kv.write = std::move(write_permissions);
    role_create.revoke = std::move(revoked);

    std::vector<uint8_t> buf;
    auto err = glz::write_json(role_create, buf);
    if(err) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, couldn't serialize {}", url, glz::nameof(err.ec));
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "revokeRolePermissions http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "revokeRolePermissions status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status == HttpStatus::unauthorized) {
        co_return tl::unexpected(EtcdError::UNAUTHORIZED);
    }
    if(http_reply->status == HttpStatus::not_found) {
        co_return tl::unexpected(EtcdError::NOT_FOUND);
    }
    if(http_reply->status == HttpStatus::conflict) {
        co_return tl::unexpected(EtcdError::DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT);
    }

    if(http_reply->status != HttpStatus::ok && http_reply->status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdRoleReply etcd_reply;
    err = glz::read_json(etcd_reply, http_reply->body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply->body.empty() ? "" : (char*)http_reply->body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<void, EtcdError>> EtcdService::deleteRole(std::string_view role) {
    std::string url = fmt::format("/v2/auth/roles/{}", role);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::delete_, url, std::move(headers), {});

    if(!http_reply) {
        ICHOR_LOG_ERROR(_logger, "deleteRole http send error {}", http_reply.error());
        co_return tl::unexpected(EtcdError::HTTP_SEND_ERROR);
    }

    ICHOR_LOG_TRACE(_logger, "deleteRole status: {}, body: {}", (int)http_reply->status, http_reply->body.empty() ? "" : (char*)http_reply->body.data());

    if(http_reply->status == HttpStatus::unauthorized) {
        co_return tl::unexpected(EtcdError::UNAUTHORIZED);
    }
    if(http_reply->status == HttpStatus::forbidden) {
        co_return tl::unexpected(EtcdError::CANNOT_DELETE_ROOT_WHILE_AUTH_IS_ENABLED);
    }
    if(http_reply->status == HttpStatus::not_found) {
        co_return tl::unexpected(EtcdError::NOT_FOUND);
    }

    if(http_reply->status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply->status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    co_return {};
}

