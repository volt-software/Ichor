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

#include <ichor/services/etcd/EtcdV3Service.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ScopeGuard.h>
#include <ichor/stl/StringUtils.h>
#include <base64/base64.h>
#include <ichor/glaze.h>
#include <glaze/util/type_traits.hpp>

using namespace Ichor::Etcd::v3;

namespace Ichor {
    struct EtcdInternalVersionReply final {
        std::string etcdserver;
        std::string etcdcluster;
    };
    struct EtcdHealthReply final {
        std::string health;
        std::optional<std::string> reason;
    };
    struct EtcdEnableAuthReply final {
        bool enabled;
    };
}

template <>
struct glz::meta<EtcdResponseHeader> {
    static constexpr auto read_cluster_id = [](EtcdResponseHeader &req, std::string const &s) {
        req.cluster_id = Ichor::FastAtoiu(s.data());
    };
    static constexpr auto write_cluster_id = [](EtcdResponseHeader const &req) -> std::string {
        return fmt::format("{}", req.cluster_id);
    };
    static constexpr auto read_member_id = [](EtcdResponseHeader &req, std::string const &s) {
        req.member_id = Ichor::FastAtoiu(s.data());
    };
    static constexpr auto write_member_id = [](EtcdResponseHeader const &req) -> std::string {
        return fmt::format("{}", req.member_id);
    };
    static constexpr auto read_revision = [](EtcdResponseHeader &req, std::string const &s) {
        req.revision = Ichor::FastAtoi(s.data());
    };
    static constexpr auto write_revision = [](EtcdResponseHeader const &req) -> std::string {
        return fmt::format("{}", req.revision);
    };
    static constexpr auto read_raft_term = [](EtcdResponseHeader &req, std::string const &s) {
        req.raft_term = Ichor::FastAtoiu(s.data());
    };
    static constexpr auto write_raft_term = [](EtcdResponseHeader const &req) -> std::string {
        return fmt::format("{}", req.raft_term);
    };

    using T = EtcdResponseHeader;
    static constexpr auto value = object(
            "cluster_id", custom<read_cluster_id, write_cluster_id>,
            "member_id", custom<read_member_id, write_member_id>,
            "revision", custom<read_revision, write_revision>,
            "raft_term", custom<read_raft_term, write_raft_term>
    );
};

template <>
struct glz::meta<EtcdPutRequest> {
    static constexpr auto read_key = [](EtcdPutRequest &req, std::string const &s) {
        req.key = base64_decode(s);
    };
    static constexpr auto write_key = [](EtcdPutRequest const &req) -> std::string {
        return base64_encode(reinterpret_cast<unsigned char const *>(req.key.c_str()), req.key.size());
    };
    static constexpr auto read_value = [](EtcdPutRequest &req, std::string const &s) {
        req.value = base64_decode(s);
    };
    static constexpr auto write_value = [](EtcdPutRequest const &req) -> std::string {
        return base64_encode(reinterpret_cast<unsigned char const *>(req.value.c_str()), req.value.size());
    };
    static constexpr auto read_lease = [](EtcdPutRequest &req, std::string const &s) {
        req.lease = Ichor::FastAtoi(s.data());
    };
    static constexpr auto write_lease = [](EtcdPutRequest const &req) -> std::string {
        return fmt::format("{}", req.lease);
    };

    using T = EtcdPutRequest;
    static constexpr auto value = object(
            "key", custom<read_key, write_key>,
            "value", custom<read_value, write_value>,
            "lease", custom<read_lease, write_lease>,
            "prev_kv", &T::prev_kv,
            "ignore_value", &T::ignore_value,
            "ignore_lease", &T::ignore_lease
    );
};

template <>
struct glz::meta<EtcdPutResponse> {
    using T = EtcdPutResponse;
    static constexpr auto value = object(
            "header", &T::header,
            "prev_kv", &T::prev_kv
    );
};

template <>
struct glz::meta<EtcdRangeRequest> {
    static constexpr auto read_key = [](EtcdRangeRequest &req, std::string const &s) {
        req.key = base64_decode(s);
    };
    static constexpr auto write_key = [](EtcdRangeRequest const &req) -> std::string {
        return base64_encode(reinterpret_cast<unsigned char const *>(req.key.c_str()), req.key.size());
    };
    static constexpr auto read_range_end = [](EtcdRangeRequest &req, std::optional<std::string> const &s) {
        if(s) {
            req.range_end = base64_decode(*s);
        }
    };
    static constexpr auto write_range_end = [](EtcdRangeRequest const &req) -> std::optional<std::string> {
        if(!req.range_end) {
            return std::nullopt;
        }
        return base64_encode(reinterpret_cast<unsigned char const *>(req.range_end->c_str()), req.range_end->size());
    };
    static constexpr auto read_limit = [](EtcdRangeRequest &req, std::string const &s) {
        req.limit = Ichor::FastAtoi(s.data());
    };
    static constexpr auto write_limit = [](EtcdRangeRequest const &req) -> std::string {
        return fmt::format("{}", req.limit);
    };
    static constexpr auto read_revision = [](EtcdRangeRequest &req, std::string const &s) {
        req.revision = Ichor::FastAtoi(s.data());
    };
    static constexpr auto write_revision = [](EtcdRangeRequest const &req) -> std::string {
        return fmt::format("{}", req.revision);
    };
    static constexpr auto read_min_mod_revision = [](EtcdRangeRequest &req, std::optional<std::string> const &s) {
        if(s) {
            req.min_mod_revision = Ichor::FastAtoi((*s).c_str());
        }
    };
    static constexpr auto write_min_mod_revision = [](EtcdRangeRequest const &req) -> std::optional<std::string> {
        if(!req.range_end) {
            return std::nullopt;
        }
        return fmt::format("{}", *req.min_mod_revision);
    };
    static constexpr auto read_max_mod_revision = [](EtcdRangeRequest &req, std::optional<std::string> const &s) {
        if(s) {
            req.max_mod_revision = Ichor::FastAtoi((*s).c_str());
        }
    };
    static constexpr auto write_max_mod_revision = [](EtcdRangeRequest const &req) -> std::optional<std::string> {
        if(!req.range_end) {
            return std::nullopt;
        }
        return fmt::format("{}", *req.max_mod_revision);
    };
    static constexpr auto read_min_create_revision = [](EtcdRangeRequest &req, std::optional<std::string> const &s) {
        if(s) {
            req.min_create_revision = Ichor::FastAtoi((*s).c_str());
        }
    };
    static constexpr auto write_min_create_revision = [](EtcdRangeRequest const &req) -> std::optional<std::string> {
        if(!req.range_end) {
            return std::nullopt;
        }
        return fmt::format("{}", *req.min_create_revision);
    };
    static constexpr auto read_max_create_revision = [](EtcdRangeRequest &req, std::optional<std::string> const &s) {
        if(s) {
            req.max_create_revision = Ichor::FastAtoi((*s).c_str());
        }
    };
    static constexpr auto write_max_create_revision = [](EtcdRangeRequest const &req) -> std::optional<std::string> {
        if(!req.range_end) {
            return std::nullopt;
        }
        return fmt::format("{}", *req.max_create_revision);
    };

    using T = EtcdRangeRequest;
    static constexpr auto value = object(
            "key", custom<read_key, write_key>,
            "range_end", custom<read_range_end, write_range_end>,
            "limit", custom<read_limit, write_limit>,
            "revision", custom<read_revision, write_revision>,
            "sort_order", &T::sort_order,
            "sort_target", &T::sort_target,
            "serializable", &T::serializable,
            "keys_only", &T::keys_only,
            "count_only", &T::count_only,
            "min_mod_revision", custom<read_min_mod_revision, write_min_mod_revision>,
            "max_mod_revision", custom<read_max_mod_revision, write_max_mod_revision>,
            "min_create_revision", custom<read_min_create_revision, write_min_create_revision>,
            "max_create_revision", custom<read_max_create_revision, write_max_create_revision>
    );
};

template <>
struct glz::meta<EtcdRangeResponse> {
    static constexpr auto read_count = [](EtcdRangeResponse &req, std::string const &s) {
        req.count = Ichor::FastAtoi(s.data());
    };
    static constexpr auto write_count = [](EtcdRangeResponse const &req) -> std::string {
        return fmt::format("{}", req.count);
    };

    using T = EtcdRangeResponse;
    static constexpr auto value = object(
            "header", &T::header,
            "kvs", &T::kvs,
            "count", custom<read_count, write_count>,
            "more", &T::more
    );
};

template <>
struct glz::meta<EtcdDeleteRangeRequest> {
    static constexpr auto read_key = [](EtcdDeleteRangeRequest &req, std::string const &s) {
        req.key = base64_decode(s);
    };
    static constexpr auto write_key = [](EtcdDeleteRangeRequest const &req) -> std::string {
        return base64_encode(reinterpret_cast<unsigned char const *>(req.key.c_str()), req.key.size());
    };
    static constexpr auto read_range_end = [](EtcdDeleteRangeRequest &req, std::optional<std::string> const &s) {
        if(s) {
            req.range_end = base64_decode(*s);
        }
    };
    static constexpr auto write_range_end = [](EtcdDeleteRangeRequest const &req) -> std::optional<std::string> {
        if(!req.range_end) {
            return std::nullopt;
        }
        return base64_encode(reinterpret_cast<unsigned char const *>(req.range_end->c_str()), req.range_end->size());
    };

    using T = EtcdDeleteRangeRequest;
    static constexpr auto value = object(
            "key", custom<read_key, write_key>,
            "range_end", custom<read_range_end, write_range_end>,
            "prev_kv", &T::prev_kv
    );
};

template <>
struct glz::meta<EtcdDeleteRangeResponse> {
    static constexpr auto read_deleted = [](EtcdDeleteRangeResponse &req, std::string const &s) {
        req.deleted = Ichor::FastAtoi(s.data());
    };
    static constexpr auto write_deleted = [](EtcdDeleteRangeResponse const &req) -> std::string {
        return fmt::format("{}", req.deleted);
    };

    using T = EtcdDeleteRangeResponse;
    static constexpr auto value = object(
            "header", &T::header,
            "deleted", custom<read_deleted, write_deleted>,
            "prev_kv", &T::prev_kvs
    );
};

template <>
struct glz::meta<EtcdKeyValue> {
    static constexpr auto read_key = [](EtcdKeyValue &req, std::string const &s) {
        req.key = base64_decode(s);
    };
    static constexpr auto write_key = [](EtcdKeyValue const &req) -> std::string {
        return base64_encode(reinterpret_cast<unsigned char const *>(req.key.c_str()), req.key.size());
    };
    static constexpr auto read_value = [](EtcdKeyValue &req, std::string const &s) {
        req.value = base64_decode(s);
    };
    static constexpr auto write_value = [](EtcdKeyValue const &req) -> std::string {
        return base64_encode(reinterpret_cast<unsigned char const *>(req.value.c_str()), req.value.size());
    };
    static constexpr auto read_create_revision = [](EtcdKeyValue &req, std::string const &s) {
        req.create_revision = Ichor::FastAtoi(s.data());
    };
    static constexpr auto write_create_revision = [](EtcdKeyValue const &req) -> std::string {
        return fmt::format("{}", req.create_revision);
    };
    static constexpr auto read_mod_revision = [](EtcdKeyValue &req, std::string const &s) {
        req.mod_revision = Ichor::FastAtoi(s.data());
    };
    static constexpr auto write_mod_revision = [](EtcdKeyValue const &req) -> std::string {
        return fmt::format("{}", req.mod_revision);
    };
    static constexpr auto read_version = [](EtcdKeyValue &req, std::string const &s) {
        req.version = Ichor::FastAtoi(s.data());
    };
    static constexpr auto write_version = [](EtcdKeyValue const &req) -> std::string {
        return fmt::format("{}", req.version);
    };
    static constexpr auto read_lease = [](EtcdKeyValue &req, std::string const &s) {
        req.lease = Ichor::FastAtoi(s.data());
    };
    static constexpr auto write_lease = [](EtcdKeyValue const &req) -> std::string {
        return fmt::format("{}", req.lease);
    };

    using T = EtcdKeyValue;
    static constexpr auto value = object(
            "key", custom<read_key, write_key>,
            "value", custom<read_value, write_value>,
            "create_revision", custom<read_create_revision, write_create_revision>,
            "mod_revision", custom<read_mod_revision, write_mod_revision>,
            "version", custom<read_version, write_version>,
            "lease", custom<read_lease, write_lease>
    );
};


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
struct glz::meta<Ichor::EtcdInternalVersionReply> {
    using T = Ichor::EtcdInternalVersionReply;
    static constexpr auto value = object(
            "etcdserver", &T::etcdserver,
            "etcdcluster", &T::etcdcluster
    );
};

EtcdService::EtcdService(DependencyRegister &reg, Properties props) : AdvancedService<EtcdService>(std::move(props)) {
    reg.registerDependency<ILogger>(this, DependencyFlags::NONE, getProperties());
    reg.registerDependency<IHttpConnectionService>(this, DependencyFlags::REQUIRED | DependencyFlags::ALLOW_MULTIPLE, getProperties());
    reg.registerDependency<IClientFactory>(this, DependencyFlags::REQUIRED);
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

    auto ver = co_await version();

    if(!ver) {
        ICHOR_LOG_ERROR(_logger, "Couldn't get version from etcd: {}", ver.error());
    }

    if((*ver).etcdserver < Version{3, 3, 0}) {
        _versionSpecificUrl = "/v3alpha";
    } else if((*ver).etcdserver < Version{3, 4, 0}) {
        _versionSpecificUrl = "/v3beta";
    }
    _detectedVersion = (*ver).etcdserver;

    ICHOR_LOG_TRACE(_logger, "Started, detected version {}", (*ver).etcdserver);
    co_return {};
}

Ichor::Task<void> EtcdService::stop() {
    ICHOR_LOG_TRACE(_logger, "Stopped");
    co_return;
}

void EtcdService::addDependencyInstance(ILogger &logger, IService &) {
    _logger = &logger;
    ICHOR_LOG_TRACE(_logger, "Added logger");
}

void EtcdService::removeDependencyInstance(ILogger&, IService&) {
    ICHOR_LOG_TRACE(_logger, "Removed logger");
    _logger = nullptr;
}

void EtcdService::addDependencyInstance(IHttpConnectionService &conn, IService &) {
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

void EtcdService::removeDependencyInstance(IHttpConnectionService &conn, IService &) {
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

void EtcdService::addDependencyInstance(IClientFactory &factory, IService &) {
    ICHOR_LOG_TRACE(_logger, "Added clientFactory");
    _clientFactory = &factory;
}

void EtcdService::removeDependencyInstance(IClientFactory&, IService&) {
    ICHOR_LOG_TRACE(_logger, "Removed clientFactory");
    _clientFactory = nullptr;
}

template <typename ReqT, typename RespT>
static Ichor::Task<tl::expected<RespT, EtcdError>> execute_request(std::string url, tl::optional<std::string> &auth, Ichor::ILogger *logger, Ichor::IHttpConnectionService* conn, ReqT const &req) {
    using namespace Ichor;
    unordered_map<std::string, std::string> headers{};

    if(auth) {
        headers.emplace("Authorization", *auth);
    }

    std::vector<uint8_t> msg_buf;
    glz::template write_json<const ReqT&, std::vector<uint8_t>&>(req, msg_buf);
    msg_buf.push_back('\0');
    ICHOR_LOG_TRACE(logger, "{} {}", url, reinterpret_cast<char *>(msg_buf.data()));

    auto http_reply = co_await conn->sendAsync(HttpMethod::post, url, std::move(headers), std::move(msg_buf));
    ICHOR_LOG_TRACE(logger, "{} status: {}, body: {}", url, (int)http_reply.status, http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    RespT etcd_reply;
    auto err = glz::template read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(logger, "json {}", http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdPutResponse, EtcdError>> EtcdService::put(EtcdPutRequest const &req) {
    if(req.prev_kv && _detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request prevKv for etcd server {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }
    if(req.ignore_value && _detectedVersion < Version{3, 2, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request ignoreValue for etcd server {}, minimum 3.2.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }
    if(req.ignore_lease && _detectedVersion < Version{3, 2, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request ignoreLease for etcd server {}, minimum 3.2.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    co_return co_await execute_request<EtcdPutRequest, EtcdPutResponse>(fmt::format("{}/kv/put", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<EtcdRangeResponse, EtcdError>> EtcdService::range(EtcdRangeRequest const &req) {
    if(req.min_mod_revision && _detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request min_mod_revision for etcd server {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }
    if(req.max_mod_revision && _detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request max_mod_revision for etcd server {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }
    if(req.min_create_revision && _detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request min_create_revision for etcd server {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }
    if(req.max_create_revision && _detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request max_create_revision for etcd server {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    co_return co_await execute_request<EtcdRangeRequest, EtcdRangeResponse>(fmt::format("{}/kv/range", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<EtcdDeleteRangeResponse, EtcdError>> EtcdService::deleteRange(EtcdDeleteRangeRequest const &req) {
    if(req.prev_kv && _detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request prev_kv for etcd server {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    co_return co_await execute_request<EtcdDeleteRangeRequest, EtcdDeleteRangeResponse>(fmt::format("{}/kv/deleterange", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<EtcdVersionReply, EtcdError>> EtcdService::version() {
    std::string url = fmt::format("/version");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});
    ICHOR_LOG_TRACE(_logger, "version status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdInternalVersionReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    auto etcdserverver = parseStringAsVersion(etcd_reply.etcdserver);

    if(!etcdserverver) {
        ICHOR_LOG_ERROR(_logger, "Error parsing etcd server version \"{}\"", etcd_reply.etcdserver);
        co_return tl::unexpected(EtcdError::VERSION_PARSE_ERROR);
    }

    auto etcdclusterver = parseStringAsVersion(etcd_reply.etcdcluster);

    if(!etcdclusterver) {
        ICHOR_LOG_ERROR(_logger, "Error parsing etcd server version \"{}\"", etcd_reply.etcdcluster);
        co_return tl::unexpected(EtcdError::VERSION_PARSE_ERROR);
    }

    co_return EtcdVersionReply{*etcdserverver, *etcdclusterver};
}

Ichor::Version EtcdService::getDetectedVersion() const {
    if(!isStarted()) [[unlikely]] {
        std::terminate();
    }
    return _detectedVersion;
}

Ichor::Task<tl::expected<bool, EtcdError>> EtcdService::health() {
    std::string url = fmt::format("/health");
    auto http_reply = co_await _mainConn->sendAsync(HttpMethod::get, url, {}, {});
    ICHOR_LOG_TRACE(_logger, "health status: {}, body: {}", (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdHealthReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply.health == "true";
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
