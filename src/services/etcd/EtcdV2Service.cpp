// We're doing some naughty stuff where we tell fmt to insert char's into a vector<uint8_t>.
// Since we have most warnings turned on and -Werror, this turns into compile errors that are actually harmless.
#include <iterator>
#if defined( __GNUC__ )
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <fmt/core.h>
#if defined( __GNUC__ )
#    pragma GCC diagnostic pop
#endif

#include <ichor/services/etcd/EtcdV2Service.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ScopeGuard.h>
#include <base64/base64.h>
#include <ichor/glaze.h>
#include <glaze/util/type_traits.hpp>

template <>
struct glz::meta<Ichor::EtcdReplyNode> {
    using T = Ichor::EtcdReplyNode;
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
struct glz::meta<Ichor::EtcdReply> {
    using T = Ichor::EtcdReply;
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
struct glz::meta<Ichor::EtcdLeaderInfoStats> {
    using T = Ichor::EtcdLeaderInfoStats;
    static constexpr auto value = object(
            "leader", &T::leader,
            "uptime", &T::uptime,
            "startTime", &T::startTime
    );
};

template <>
struct glz::meta<Ichor::EtcdSelfStats> {
    using T = Ichor::EtcdSelfStats;
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
struct glz::meta<Ichor::EtcdStoreStats> {
    using T = Ichor::EtcdStoreStats;
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

namespace Ichor {
    struct EtcdHealthReply final {
        std::string health;
        std::optional<std::string> reason;
    };
    struct EtcdEnableAuthReply final {
        bool enabled;
    };
}


template <>
struct glz::meta<Ichor::EtcdHealthReply> {
    using T = Ichor::EtcdHealthReply;
    static constexpr auto value = object(
            "health", &T::health,
            "reason", &T::reason
    );
};

template <>
struct glz::meta<Ichor::EtcdEnableAuthReply> {
    using T = Ichor::EtcdEnableAuthReply;
    static constexpr auto value = object(
            "enabled", &T::enabled
    );
};

template <>
struct glz::meta<Ichor::EtcdVersionReply> {
    using T = Ichor::EtcdVersionReply;
    static constexpr auto value = object(
            "etcdserver", &T::etcdserver,
            "etcdcluster", &T::etcdcluster
    );
};

template <>
struct glz::meta<Ichor::EtcdKvReply> {
    using T = Ichor::EtcdKvReply;
    static constexpr auto value = object(
            "read", &T::read,
            "write", &T::write
    );
};

template <>
struct glz::meta<Ichor::EtcdPermissionsReply> {
    using T = Ichor::EtcdPermissionsReply;
    static constexpr auto value = object(
            "kv", &T::kv
    );
};

template <>
struct glz::meta<Ichor::EtcdRoleReply> {
    using T = Ichor::EtcdRoleReply;
    static constexpr auto value = object(
            "role", &T::role,
            "permissions", &T::permissions,
            "grant", &T::grant,
            "revoke", &T::revoke
    );
};

template <>
struct glz::meta<Ichor::EtcdRolesReply> {
    using T = Ichor::EtcdRolesReply;
    static constexpr auto value = object(
            "roles", &T::roles
    );
};

template <>
struct glz::meta<Ichor::EtcdUserReply> {
    using T = Ichor::EtcdUserReply;
    static constexpr auto value = object(
            "user", &T::user,
            "password", &T::password,
            "roles", &T::roles,
            "grant", &T::grant,
            "revoke", &T::revoke
    );
};

template <>
struct glz::meta<Ichor::EtcdUpdateUserReply> {
    using T = Ichor::EtcdUpdateUserReply;
    static constexpr auto value = object(
            "user", &T::user,
            "roles", &T::roles
    );
};

template <>
struct glz::meta<Ichor::EtcdUsersReply> {
    using T = Ichor::EtcdUsersReply;
    static constexpr auto value = object(
            "users", &T::users
    );
};

Ichor::EtcdV2Service::EtcdV2Service(DependencyRegister &reg, Properties props) : AdvancedService<EtcdV2Service>(std::move(props)) {
    reg.registerDependency<ILogger>(this, DependencyFlags::NONE, getProperties());
    reg.registerDependency<IHttpConnectionService>(this, DependencyFlags::REQUIRED | DependencyFlags::ALLOW_MULTIPLE, getProperties());
    reg.registerDependency<IClientFactory>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::EtcdV2Service::start() {
    ICHOR_LOG_TRACE(_logger, "Started");
    co_return {};
}

Ichor::Task<void> Ichor::EtcdV2Service::stop() {
    ICHOR_LOG_TRACE(_logger, "Stopped");
    co_return;
}

void Ichor::EtcdV2Service::addDependencyInstance(ILogger &logger, IService &) {
    _logger = &logger;
    ICHOR_LOG_TRACE(_logger, "Added logger");
}

void Ichor::EtcdV2Service::removeDependencyInstance(ILogger&, IService&) {
    ICHOR_LOG_TRACE(_logger, "Removed logger");
    _logger = nullptr;
}

void Ichor::EtcdV2Service::addDependencyInstance(IHttpConnectionService &conn, IService &) {
    if(_mainConn == nullptr) {
        ICHOR_LOG_TRACE(_logger, "Added MainCon");
        _mainConn = &conn;
        return;
    }

    // In a condition where we're stopping, e.g. the mainConn is already gone, the existing requests may still get 'added', but we've already cleaned up all requests
    if(_connRequests.empty()) {
        return;
    }

    ICHOR_LOG_TRACE(_logger, "Added conn, triggering coroutine");

    _connRequests.top().conn = &conn;
    _connRequests.top().event.set();
}

void Ichor::EtcdV2Service::removeDependencyInstance(IHttpConnectionService &conn, IService &) {
    if(_mainConn == &conn) {
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

void Ichor::EtcdV2Service::addDependencyInstance(IClientFactory &factory, IService &) {
    ICHOR_LOG_TRACE(_logger, "Added clientFactory");
    _clientFactory = &factory;
}

void Ichor::EtcdV2Service::removeDependencyInstance(IClientFactory&, IService&) {
    ICHOR_LOG_TRACE(_logger, "Removed clientFactory");
    _clientFactory = nullptr;
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdV2Service::put(std::string_view key, std::string_view value, tl::optional<std::string_view> previous_value, tl::optional<uint64_t> previous_index, tl::optional<bool> previous_exists, tl::optional<uint64_t> ttl_second, bool refresh, bool dir, bool in_order) {
    std::vector<uint8_t> msg_buf;
    fmt::format_to(std::back_inserter(msg_buf), "value={}", value);
    if(ttl_second) {
        fmt::format_to(std::back_inserter(msg_buf), "&ttl={}", *ttl_second);
    }
    if(refresh) {
        fmt::format_to(std::back_inserter(msg_buf), "&refresh=true");
    }
    if(previous_value) {
        fmt::format_to(std::back_inserter(msg_buf), "&prevValue={}", *previous_value);
    }
    if(previous_index) {
        fmt::format_to(std::back_inserter(msg_buf), "&prevIndex={}", *previous_index);
    }
    if(previous_exists) {
        fmt::format_to(std::back_inserter(msg_buf), "&prevExist={}", (*previous_exists) ? "true" : "false");
    }
    if(dir) {
        fmt::format_to(std::back_inserter(msg_buf), "&dir=true");
    }
    ICHOR_LOG_TRACE(_logger, "put body {}", std::string_view{(char*)msg_buf.data(), msg_buf.size()});
    unordered_map<std::string, std::string> headers{{"Content-Type", "application/x-www-form-urlencoded"}};

    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }
    HttpMethod method = in_order ? HttpMethod::post : HttpMethod::put;
    auto http_reply = co_await _mainConn->sendAsync(method, fmt::format("/v2/keys/{}", key), std::move(headers), std::move(msg_buf));
    ICHOR_LOG_TRACE(_logger, "put status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::created && http_reply.status != HttpStatus::forbidden && http_reply.status != HttpStatus::precondition_failed && http_reply.status != HttpStatus::not_found) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", fmt::format("/v2/keys/{}", key), (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    auto cluster_id = http_reply.headers.find("X-Etcd-Cluster-Id");
    auto etcd_index = http_reply.headers.find("X-Etcd-Index");
    auto raft_index = http_reply.headers.find("X-Raft-Index");
    auto raft_term = http_reply.headers.find("X-Raft-Term");

    if(cluster_id != end(http_reply.headers)) {
        etcd_reply.x_etcd_cluster_id = cluster_id->second;
    }
    if(etcd_index != end(http_reply.headers)) {
        etcd_reply.x_etcd_index = Ichor::FastAtoiu(etcd_index->second.c_str());
    }
    if(raft_index != end(http_reply.headers)) {
        etcd_reply.x_raft_index = Ichor::FastAtoiu(raft_index->second.c_str());
    }
    if(raft_term != end(http_reply.headers)) {
        etcd_reply.x_raft_term = Ichor::FastAtoiu(raft_term->second.c_str());
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdV2Service::put(std::string_view key, std::string_view value, tl::optional<uint64_t> ttl_second, bool refresh) {
    return put(key, value, {}, {}, {}, ttl_second, refresh, false, false);
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdV2Service::put(std::string_view key, std::string_view value, tl::optional<uint64_t> ttl_second) {
    return put(key, value, {}, {}, {}, ttl_second, false, false, false);
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdV2Service::put(std::string_view key, std::string_view value) {
    return put(key, value, {}, {}, {}, {}, false, false, false);
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdV2Service::get(std::string_view key, bool recursive, bool sorted, bool watch, tl::optional<uint64_t> watchIndex) {
    std::string url{fmt::format("/v2/keys/{}?", key)};

    // using watches blocks the connection and thus blocks every other call
    // This implementation requests a new HttpConnection specifically for this and cleans it up afterwards
    IHttpConnectionService *connToUse = _mainConn;
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
    ICHOR_LOG_TRACE(_logger, "get status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::not_found) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    if(http_reply.body.empty() && http_reply.status == HttpStatus::ok) {
        co_return tl::unexpected(Ichor::EtcdError::CONNECTION_CLOSED_PREMATURELY_TRY_AGAIN);
    }

    EtcdReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdV2Service::get(std::string_view key) {
    return get(key, false, false, false, {});
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdV2Service::del(std::string_view key, bool recursive, bool dir) {
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
    ICHOR_LOG_TRACE(_logger, "del status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdV2Service::leaderStatistics() {
    std::string url = fmt::format("/v2/stats/leader");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});
    ICHOR_LOG_ERROR(_logger, "leaderStatistics json {}", http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());
    co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
}

Ichor::Task<tl::expected<Ichor::EtcdSelfStats, Ichor::EtcdError>> Ichor::EtcdV2Service::selfStatistics() {
    std::string url = fmt::format("/v2/stats/self");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});
    ICHOR_LOG_TRACE(_logger, "selfStatistics status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdSelfStats etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdStoreStats, Ichor::EtcdError>> Ichor::EtcdV2Service::storeStatistics() {
    std::string url = fmt::format("/v2/stats/store");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});
    ICHOR_LOG_TRACE(_logger, "storeStatistics status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdStoreStats etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdVersionReply, Ichor::EtcdError>> Ichor::EtcdV2Service::version() {
    std::string url = fmt::format("/version");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});
    ICHOR_LOG_TRACE(_logger, "version status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdVersionReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<bool, Ichor::EtcdError>> Ichor::EtcdV2Service::health() {
    std::string url = fmt::format("/health");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});
    ICHOR_LOG_TRACE(_logger, "health status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdHealthReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply.health == "true";
}

Ichor::Task<tl::expected<bool, Ichor::EtcdError>> Ichor::EtcdV2Service::authStatus() {
    std::string url = fmt::format("/v2/auth/enable");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});
    ICHOR_LOG_TRACE(_logger, "authStatus status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdEnableAuthReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply.enabled;
}

Ichor::Task<tl::expected<bool, Ichor::EtcdError>> Ichor::EtcdV2Service::enableAuth() {
    std::string url = fmt::format("/v2/auth/enable");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, {}, {});
    ICHOR_LOG_TRACE(_logger, "enableAuth status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status == HttpStatus::bad_request) {
        co_return tl::unexpected(Ichor::EtcdError::ROOT_USER_NOT_YET_CREATED);
    }

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::conflict) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    co_return true;
}

Ichor::Task<tl::expected<bool, Ichor::EtcdError>> Ichor::EtcdV2Service::disableAuth() {
    std::string url = fmt::format("/v2/auth/enable");

    if(!_auth) {
        co_return tl::unexpected(Ichor::EtcdError::NO_AUTHENTICATION_SET);
    }

    // forced to declare as a variable due to internal compiler errors in gcc <= 12.3.0
    unordered_map<std::string, std::string> headers{{"Authorization", *_auth}};
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::delete_, url, std::move(headers), {});
    ICHOR_LOG_TRACE(_logger, "disableAuth status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status == HttpStatus::unauthorized) {
        co_return tl::unexpected(Ichor::EtcdError::UNAUTHORIZED);
    }

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::conflict) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    co_return true;
}

Ichor::Task<tl::expected<Ichor::EtcdUsersReply, Ichor::EtcdError>> Ichor::EtcdV2Service::getUsers() {
    std::string url = fmt::format("/v2/auth/users");

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, std::move(headers), {});
    ICHOR_LOG_TRACE(_logger, "getUsers status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdUsersReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdUserReply, Ichor::EtcdError>> Ichor::EtcdV2Service::getUserDetails(std::string_view user) {
    std::string url = fmt::format("/v2/auth/users/{}", user);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, std::move(headers), {});
    ICHOR_LOG_TRACE(_logger, "getUserDetails status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdUserReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

void Ichor::EtcdV2Service::setAuthentication(std::string_view user, std::string_view pass) {
    _auth = fmt::format("{}:{}", user, pass);
    _auth = base64_encode(reinterpret_cast<unsigned const char*>(_auth->c_str()), _auth->length());
    _auth = fmt::format("Basic {}", *_auth);
}

void Ichor::EtcdV2Service::clearAuthentication() {
    _auth.reset();
}

tl::optional<std::string> Ichor::EtcdV2Service::getAuthenticationUser() const {
    if(!_auth) {
        return {};
    }

    auto full_str = base64_decode((*_auth).substr(6));
    auto pos = full_str.find(':');
    return full_str.substr(0, pos);
}

Ichor::Task<tl::expected<Ichor::EtcdUpdateUserReply, Ichor::EtcdError>> Ichor::EtcdV2Service::createUser(std::string_view user, std::string_view pass) {
    std::string url = fmt::format("/v2/auth/users/{}", user);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    EtcdUserReply user_create{};
    user_create.user = user;
    user_create.password = pass;

    std::vector<uint8_t> buf;
    glz::write_json(user_create, buf);
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));
    ICHOR_LOG_TRACE(_logger, "createUser status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdUpdateUserReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdUpdateUserReply, Ichor::EtcdError>> Ichor::EtcdV2Service::grantUserRoles(std::string_view user, std::vector<std::string> roles) {
    std::string url = fmt::format("/v2/auth/users/{}", user);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    EtcdUserReply user_create{};
    user_create.user = user;
    user_create.grant = std::move(roles);

    std::vector<uint8_t> buf;
    glz::write_json(user_create, buf);
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));
    ICHOR_LOG_TRACE(_logger, "grantUserRoles status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdUpdateUserReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdUpdateUserReply, Ichor::EtcdError>> Ichor::EtcdV2Service::revokeUserRoles(std::string_view user, std::vector<std::string> roles) {
    std::string url = fmt::format("/v2/auth/users/{}", user);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    EtcdUserReply user_create{};
    user_create.user = user;
    user_create.revoke = std::move(roles);

    std::vector<uint8_t> buf;
    glz::write_json(user_create, buf);
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));
    ICHOR_LOG_TRACE(_logger, "revokeUserRoles status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdUpdateUserReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdUpdateUserReply, Ichor::EtcdError>> Ichor::EtcdV2Service::updateUserPassword(std::string_view user, std::string_view pass) {
    std::string url = fmt::format("/v2/auth/users/{}", user);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    EtcdUserReply user_create{};
    user_create.user = user;
    user_create.password = pass;

    std::vector<uint8_t> buf;
    glz::write_json(user_create, buf);
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));
    ICHOR_LOG_TRACE(_logger, "updateUserPassword status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdUpdateUserReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<void, Ichor::EtcdError>> Ichor::EtcdV2Service::deleteUser(std::string_view user) {
    std::string url = fmt::format("/v2/auth/users/{}", user);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::delete_, url, std::move(headers), {});
    ICHOR_LOG_TRACE(_logger, "deleteUser status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status == HttpStatus::unauthorized) {
        co_return tl::unexpected(Ichor::EtcdError::UNAUTHORIZED);
    }
    if(http_reply.status == HttpStatus::forbidden) {
        co_return tl::unexpected(Ichor::EtcdError::REMOVING_ROOT_NOT_ALLOWED_WITH_AUTH_ENABLED);
    }
    if(http_reply.status == HttpStatus::not_found) {
        co_return tl::unexpected(Ichor::EtcdError::NOT_FOUND);
    }

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    co_return {};
}

Ichor::Task<tl::expected<Ichor::EtcdRolesReply, Ichor::EtcdError>> Ichor::EtcdV2Service::getRoles() {
    std::string url = fmt::format("/v2/auth/roles");

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, std::move(headers), {});
    ICHOR_LOG_TRACE(_logger, "getRoles status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status == HttpStatus::unauthorized) {
        co_return tl::unexpected(Ichor::EtcdError::UNAUTHORIZED);
    }

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdRolesReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdRoleReply, Ichor::EtcdError>> Ichor::EtcdV2Service::getRole(std::string_view role) {
    std::string url = fmt::format("/v2/auth/roles/{}", role);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, std::move(headers), {});
    ICHOR_LOG_TRACE(_logger, "getRole status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status == HttpStatus::unauthorized) {
        co_return tl::unexpected(Ichor::EtcdError::UNAUTHORIZED);
    }
    if(http_reply.status == HttpStatus::not_found) {
        co_return tl::unexpected(Ichor::EtcdError::NOT_FOUND);
    }

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdRoleReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdRoleReply, Ichor::EtcdError>> Ichor::EtcdV2Service::createRole(std::string_view role, std::vector<std::string> read_permissions, std::vector<std::string> write_permissions) {
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
    glz::write_json(role_create, buf);
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));
    ICHOR_LOG_TRACE(_logger, "createRole status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status == HttpStatus::unauthorized) {
        co_return tl::unexpected(Ichor::EtcdError::UNAUTHORIZED);
    }
    if(http_reply.status == HttpStatus::not_found) {
        co_return tl::unexpected(Ichor::EtcdError::NOT_FOUND);
    }
    if(http_reply.status == HttpStatus::conflict) {
        co_return tl::unexpected(Ichor::EtcdError::DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT);
    }

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdRoleReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdRoleReply, Ichor::EtcdError>> Ichor::EtcdV2Service::grantRolePermissions(std::string_view role, std::vector<std::string> read_permissions, std::vector<std::string> write_permissions) {
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
    glz::write_json(role_create, buf);
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));
    ICHOR_LOG_TRACE(_logger, "grantRolePermissions status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status == HttpStatus::unauthorized) {
        co_return tl::unexpected(Ichor::EtcdError::UNAUTHORIZED);
    }
    if(http_reply.status == HttpStatus::not_found) {
        co_return tl::unexpected(Ichor::EtcdError::NOT_FOUND);
    }
    if(http_reply.status == HttpStatus::conflict) {
        co_return tl::unexpected(Ichor::EtcdError::DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT);
    }

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdRoleReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdRoleReply, Ichor::EtcdError>> Ichor::EtcdV2Service::revokeRolePermissions(std::string_view role, std::vector<std::string> read_permissions, std::vector<std::string> write_permissions) {
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
    glz::write_json(role_create, buf);
    buf.push_back('\0');

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::put, url, std::move(headers), std::move(buf));
    ICHOR_LOG_TRACE(_logger, "revokeRolePermissions status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status == HttpStatus::unauthorized) {
        co_return tl::unexpected(Ichor::EtcdError::UNAUTHORIZED);
    }
    if(http_reply.status == HttpStatus::not_found) {
        co_return tl::unexpected(Ichor::EtcdError::NOT_FOUND);
    }
    if(http_reply.status == HttpStatus::conflict) {
        co_return tl::unexpected(Ichor::EtcdError::DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT);
    }

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::created) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdRoleReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<void, Ichor::EtcdError>> Ichor::EtcdV2Service::deleteRole(std::string_view role) {
    std::string url = fmt::format("/v2/auth/roles/{}", role);

    unordered_map<std::string, std::string> headers{};
    if(_auth) {
        headers.emplace("Authorization", *_auth);
    }

    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::delete_, url, std::move(headers), {});
    ICHOR_LOG_TRACE(_logger, "deleteRole status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status == HttpStatus::unauthorized) {
        co_return tl::unexpected(Ichor::EtcdError::UNAUTHORIZED);
    }
    if(http_reply.status == HttpStatus::forbidden) {
        co_return tl::unexpected(Ichor::EtcdError::CANNOT_DELETE_ROOT_WHILE_AUTH_IS_ENABLED);
    }
    if(http_reply.status == HttpStatus::not_found) {
        co_return tl::unexpected(Ichor::EtcdError::NOT_FOUND);
    }

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    co_return {};
}

