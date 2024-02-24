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

namespace {
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
    using T = EtcdResponseHeader;
    static constexpr auto value = object(
        "cluster_id", quoted_num<&T::cluster_id>,
        "member_id", quoted_num<&T::member_id>,
        "revision", quoted_num<&T::revision>,
        "raft_term", quoted_num<&T::raft_term>
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

    using T = EtcdPutRequest;
    static constexpr auto value = object(
        "key", custom<read_key, write_key>,
        "value", custom<read_value, write_value>,
        "lease", quoted_num<&T::lease>,
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

    using T = EtcdRangeRequest;
    static constexpr auto value = object(
        "key", custom<read_key, write_key>,
        "range_end", custom<read_range_end, write_range_end>,
        "limit", quoted_num<&T::limit>,
        "revision", quoted_num<&T::revision>,
        "sort_order", &T::sort_order,
        "sort_target", &T::sort_target,
        "serializable", &T::serializable,
        "keys_only", &T::keys_only,
        "count_only", &T::count_only,
        "min_mod_revision", quoted_num<&T::min_mod_revision>,
        "max_mod_revision", quoted_num<&T::max_mod_revision>,
        "min_create_revision", quoted_num<&T::min_create_revision>,
        "max_create_revision", quoted_num<&T::max_create_revision>
    );
};

template <>
struct glz::meta<EtcdRangeResponse> {
    using T = EtcdRangeResponse;
    static constexpr auto value = object(
        "header", &T::header,
        "kvs", &T::kvs,
        "count", quoted_num<&T::count>,
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

    using T = EtcdKeyValue;
    static constexpr auto value = object(
        "key", custom<read_key, write_key>,
        "value", custom<read_value, write_value>,
        "create_revision", quoted_num<&T::create_revision>,
        "mod_revision", quoted_num<&T::mod_revision>,
        "version", quoted_num<&T::version>,
        "lease", quoted_num<&T::lease>
    );
};

template <>
struct glz::meta<EtcdCompare> {
    static constexpr auto read_key = [](EtcdCompare &req, std::string const &s) {
        req.key = base64_decode(s);
    };
    static constexpr auto write_key = [](EtcdCompare const &req) -> std::string {
        return base64_encode(reinterpret_cast<unsigned char const *>(req.key.c_str()), req.key.size());
    };
    static constexpr auto read_range_end = [](EtcdCompare &req, std::optional<std::string> const &s) {
        if(s) {
            req.range_end = base64_decode(*s);
        }
    };
    static constexpr auto write_range_end = [](EtcdCompare const &req) -> std::optional<std::string> {
        if(!req.range_end) {
            return std::nullopt;
        }
        return base64_encode(reinterpret_cast<unsigned char const *>(req.range_end->c_str()), req.range_end->size());
    };
    static constexpr auto read_value = [](EtcdCompare &req, std::optional<std::string> const &s) {
        if(s) {
            req.value = base64_decode(*s);
        }
    };
    static constexpr auto write_value = [](EtcdCompare const &req) -> std::optional<std::string> {
        if(!req.value) {
            return std::nullopt;
        }
        return base64_encode(reinterpret_cast<unsigned char const *>(req.value->c_str()), req.value->size());
    };

    using T = EtcdCompare;
    static constexpr auto value = object(
        "result", &T::result,
        "target", &T::target,
        "key", custom<read_key, write_key>,
        "range_end", custom<read_range_end, write_range_end>,
        "version", quoted_num<&T::version>,
        "create_revision", quoted_num<&T::create_revision>,
        "mod_revision", quoted_num<&T::mod_revision>,
        "value", custom<read_value, write_value>,
        "lease", quoted_num<&T::lease>
    );
};

template <>
struct glz::meta<EtcdRequestOp> {
    using T = EtcdRequestOp;
    static constexpr auto value = object(
        "request_range", &T::request_range,
        "request_put", &T::request_put,
        "request_delete", &T::request_delete,
        "request_txn", &T::request_txn
    );
};

template <>
struct glz::meta<EtcdResponseOp> {
    using T = EtcdResponseOp;
    static constexpr auto value = object(
        "response_range", &T::response_range,
        "response_put", &T::response_put,
        "response_delete_range", &T::response_delete_range,
        "response_txn", &T::response_txn
    );
};

template <>
struct glz::meta<EtcdTxnRequest> {
    using T = EtcdTxnRequest;
    static constexpr auto value = object(
        "compare", &T::compare,
        "success", &T::success,
        "failure", &T::failure
    );
};

template <>
struct glz::meta<EtcdTxnResponse> {
    using T = EtcdTxnResponse;
    static constexpr auto value = object(
        "header", &T::header,
        "succeeded", &T::succeeded,
        "responses", &T::responses
    );
};


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
struct glz::meta<EtcdInternalVersionReply> {
    using T = EtcdInternalVersionReply;
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
    glz::template write<glz::opts{.skip_null_members = true, .error_on_const_read = true}, const ReqT&, std::vector<uint8_t>&>(req, msg_buf);
    msg_buf.push_back('\0');
    ICHOR_LOG_TRACE(logger, "{} {}", url, reinterpret_cast<char *>(msg_buf.data()));

    auto http_reply = co_await conn->sendAsync(HttpMethod::post, url, std::move(headers), std::move(msg_buf));
    ICHOR_LOG_TRACE(logger, "{} status: {}, body: {}", url, (int)http_reply.status, http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    RespT etcd_reply;
    auto err = glz::template read<glz::opts{.error_on_const_read = true}>(etcd_reply, http_reply.body);

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

Ichor::Task<tl::expected<EtcdTxnResponse, EtcdError>> EtcdService::txn(EtcdTxnRequest const &req) {
    for(auto const &comp : req.compare) {
        if(comp.result == EtcdCompareResult::NOT_EQUAL && _detectedVersion < Version{3, 1, 0}) {
            ICHOR_LOG_ERROR(_logger, "Cannot request EtcdCompareResult::NOT_EQUAL for etcd server {}, minimum 3.1.0 required", _detectedVersion);
            co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
        }
        if(comp.target == EtcdCompareTarget::LEASE && _detectedVersion < Version{3, 3, 0}) {
            ICHOR_LOG_ERROR(_logger, "Cannot request EtcdCompareTarget::LEASE for etcd server {}, minimum 3.3.0 required", _detectedVersion);
            co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
        }
        if(comp.lease && _detectedVersion < Version{3, 3, 0}) {
            ICHOR_LOG_ERROR(_logger, "Cannot request lease for etcd server {}, minimum 3.3.0 required", _detectedVersion);
            co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
        }
        if(comp.range_end && _detectedVersion < Version{3, 3, 0}) {
            ICHOR_LOG_ERROR(_logger, "Cannot request range_end for etcd server {}, minimum 3.3.0 required", _detectedVersion);
            co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
        }
    }
    for(auto const &op : req.success) {
        if(op.request_txn && _detectedVersion < Version{3, 3, 0}) {
            ICHOR_LOG_ERROR(_logger, "Cannot request request_txn for etcd server {}, minimum 3.3.0 required", _detectedVersion);
            co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
        }
    }
    for(auto const &op : req.failure) {
        if(op.request_txn && _detectedVersion < Version{3, 3, 0}) {
            ICHOR_LOG_ERROR(_logger, "Cannot request request_txn for etcd server {}, minimum 3.3.0 required", _detectedVersion);
            co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
        }
    }

    co_return co_await execute_request<EtcdTxnRequest, EtcdTxnResponse>(fmt::format("{}/kv/txn", _versionSpecificUrl), _auth, _logger, _mainConn, req);
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