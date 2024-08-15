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
        std::optional<std::string> storage;
    };
    struct EtcdHealthReply final {
        std::string health;
        std::optional<std::string> reason;
    };
    struct EtcdEnableAuthReply final {
        bool enabled;
    };

    // Etcd version 3.3.X and lower contain a bug regarding parsing Key/RangeEnd. See https://github.com/etcd-io/etcd/issues/9424 for more information.
    struct AuthRoleRevokePermissionRequestBugWorkaround final {
        AuthRoleRevokePermissionRequestBugWorkaround() = delete;
        explicit AuthRoleRevokePermissionRequestBugWorkaround(AuthRoleRevokePermissionRequest const &r) : role(r.role), key(r.key), range_end(r.range_end) {}
        std::string role;
        std::string key;
        tl::optional<std::string> range_end;
    };

    // treat a value as base64
    template <class T>
    struct Base64StringType final {
        static constexpr bool glaze_wrapper = true;
        using value_type = T;
        T& val;
    };

    // treat a value as an optional
    template <class T>
    struct OptionalType final {
        static constexpr bool glaze_wrapper = true;
        using value_type = T;
        T& val;
    };

    // treat a value as a stringed enum (string to parse, underlying value as serialize)
    template <class T>
    struct StringEnumType final {
        static constexpr bool glaze_wrapper = true;
        using value_type = T;
        T& val;
    };

    // treat a value as an optional base64
    template <class T>
    struct OptionalBase64StringType final {
        static constexpr bool glaze_wrapper = true;
        using value_type = T;
        T& val;
    };

    // treat a value as vector of base64 strings
    template <class T>
    struct VectorBase64StringType final {
        static constexpr bool glaze_wrapper = true;
        using value_type = T;
        T& val;
    };
}

namespace glz::detail {
    template <>
    struct from_json<tl::optional<bool>> {
        template <auto Opts>
        static void op(tl::optional<bool>& value, auto&&... args) {
            bool val;
            read<json>::op<Opts>(val, args...);
            value = val;
        }
    };

    template <>
    struct to_json<tl::optional<bool>> {
        template <auto Opts>
        static void op(tl::optional<bool> const & value, auto&&... args) noexcept {
            if(value) {
                write<json>::op<Opts>(*value, args...);
            }
        }
    };

    template <>
    struct from_json<tl::optional<int64_t>> {
        template <auto Opts>
        static void op(tl::optional<int64_t>& value, auto&&... args) {
            int64_t val;
            read<json>::op<Opts>(val, args...);
            value = val;
        }
    };

    template <>
    struct to_json<tl::optional<int64_t>> {
        template <auto Opts>
        static void op(tl::optional<int64_t> const & value, auto&&... args) noexcept {
            if(value) {
                write<json>::op<Opts>(*value, args...);
            }
        }
    };

    template <>
    struct from_json<tl::optional<std::string>> {
        template <auto Opts>
        static void op(tl::optional<std::string>& value, auto&&... args) {
            std::string val;
            read<json>::op<Opts>(val, args...);
            value = val;
        }
    };

    template <>
    struct to_json<tl::optional<std::string>> {
        template <auto Opts>
        static void op(tl::optional<std::string> const & value, auto&&... args) noexcept {
            if(value) {
                write<json>::op<Opts>(*value, args...);
            }
        }
    };

    template <class T>
    struct from_json<Base64StringType<T>> {
        template <auto Opts>
        static void op(auto&& value, auto&&... args) {
            read<json>::op<Opts>(value.val, args...);
            value.val = base64_decode(value.val);
        }
    };

    template <class T>
    struct to_json<Base64StringType<T>> {
        template <auto Opts>
        static void op(Base64StringType<T> const & value, auto&&... args) noexcept {
            write<json>::op<Opts>(base64_encode(reinterpret_cast<unsigned char const *>(value.val.c_str()), value.val.size()), args...);
        }
    };

    template <class T>
    struct from_json<OptionalType<T>> {
        template <auto Opts>
        static void op(OptionalType<T>&& value, auto&&... args) {
            read<json>::op<Opts>(*value.val, args...);
        }
    };

    template <class T>
    struct to_json<OptionalType<T>> {
        template <auto Opts>
        static void op(OptionalType<T> const & value, auto&&... args) noexcept {
            if(value.val) {
                write<json>::op<Opts>(*value.val, args...);
            }
        }
    };

    template <class T>
    struct from_json<StringEnumType<T>>;

    template <class T>
    struct to_json<StringEnumType<T>>;

    template <>
    struct from_json<StringEnumType<EtcdAuthPermissionType>> {
        template <auto Opts>
        static void op(StringEnumType<EtcdAuthPermissionType>&& value, auto&&... args) {
            std::string val;
            read<json>::op<Opts>(val, args...);
            if(val == "READ") {
                value.val = EtcdAuthPermissionType::READ;
            } else if(val == "WRITE") {
                value.val = EtcdAuthPermissionType::WRITE;
            } else if(val == "READWRITE") {
                value.val = EtcdAuthPermissionType::READWRITE;
            } else {
                fmt::print("Unknown value for EtcdAuthPermissionType\n");
            }
        }
    };

    template <>
    struct to_json<StringEnumType<EtcdAuthPermissionType const>> {
        template <auto Opts>
        static void op(StringEnumType<EtcdAuthPermissionType const> const & value, auto&&... args) noexcept {
            write<json>::op<Opts>(value.val, args...);
        }
    };

    template <class T>
    struct from_json<OptionalBase64StringType<T>> {
        template <auto Opts>
        static void op(OptionalBase64StringType<T>&& value, auto&&... args) {
            read<json>::op<Opts>(*value.val, args...);
            value.val = base64_decode(*value.val);
        }
    };

    template <class T>
    struct to_json<OptionalBase64StringType<T>> {
        template <auto Opts>
        static void op(OptionalBase64StringType<T> const & value, auto&&... args) noexcept {
            if(value.val) {
                write<json>::op<Opts>(base64_encode(reinterpret_cast<unsigned char const *>(value.val->c_str()), value.val->size()), args...);
            }
        }
    };

    template <class T>
    struct from_json<VectorBase64StringType<T>> {
        template <auto Opts>
        static void op(VectorBase64StringType<T>&& value, auto&&... args) {
            read<json>::op<Opts>(value.val, args...);
            for(auto &val : value.val) {
                val = base64_decode(val);
            }
        }
    };

    template <class T>
    struct to_json<VectorBase64StringType<T>> {
        template <auto Opts>
        static void op(VectorBase64StringType<T> const & value, auto&&... args) noexcept {
            for(auto &val : value.val) {
                write<json>::op<Opts>(base64_encode(reinterpret_cast<unsigned char const *>(val.c_str()), val.size()), args...);
            }
        }
    };

    template <auto MemPtr>
    inline constexpr decltype(auto) Base64StringImpl() noexcept {
        return [](auto&& val) { return Base64StringType<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
    }

    template <auto MemPtr>
    inline constexpr decltype(auto) OptionalImpl() noexcept {
        return [](auto&& val) { return OptionalType<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
    }

    template <auto MemPtr>
    inline constexpr decltype(auto) StringEnumImpl() noexcept {
        return [](auto&& val) { return StringEnumType<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
    }

    template <auto MemPtr>
    inline constexpr decltype(auto) OptionalBase64StringImpl() noexcept {
        return [](auto&& val) { return OptionalBase64StringType<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
    }

    template <auto MemPtr>
    inline constexpr decltype(auto) VectorBase64StringImpl() noexcept {
        return [](auto&& val) { return VectorBase64StringType<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
    }
}

template <auto MemPtr>
constexpr auto Base64String = glz::detail::Base64StringImpl<MemPtr>();

template <auto MemPtr>
constexpr auto Optional = glz::detail::OptionalImpl<MemPtr>();

template <auto MemPtr>
constexpr auto StringEnum = glz::detail::StringEnumImpl<MemPtr>();

template <auto MemPtr>
constexpr auto OptionalBase64String = glz::detail::OptionalBase64StringImpl<MemPtr>();

template <auto MemPtr>
constexpr auto VectorBase64String = glz::detail::VectorBase64StringImpl<MemPtr>();

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
    using T = EtcdPutRequest;
    static constexpr auto value = object(
        "key", Base64String<&T::key>,
        "value", Base64String<&T::value>,
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
    using T = EtcdRangeRequest;
    static constexpr auto value = object(
        "key", Base64String<&T::key>,
        "range_end", OptionalBase64String<&T::range_end>,
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
    using T = EtcdDeleteRangeRequest;
    static constexpr auto value = object(
        "key", Base64String<&T::key>,
        "range_end", OptionalBase64String<&T::range_end>,
        "prev_kv", &T::prev_kv
    );
};

template <>
struct glz::meta<EtcdDeleteRangeResponse> {
    using T = EtcdDeleteRangeResponse;
    static constexpr auto value = object(
        "header", &T::header,
        "deleted", quoted_num<&T::deleted>,
        "prev_kv", &T::prev_kvs
    );
};

template <>
struct glz::meta<EtcdKeyValue> {
    using T = EtcdKeyValue;
    static constexpr auto value = object(
        "key", Base64String<&T::key>,
        "value", Base64String<&T::value>,
        "create_revision", quoted_num<&T::create_revision>,
        "mod_revision", quoted_num<&T::mod_revision>,
        "version", quoted_num<&T::version>,
        "lease", quoted_num<&T::lease>
    );
};

template <>
struct glz::meta<EtcdCompare> {
    using T = EtcdCompare;
    static constexpr auto value = object(
        "result", &T::result,
        "target", &T::target,
        "key", Base64String<&T::key>,
        "range_end", OptionalBase64String<&T::range_end>,
        "version", quoted_num<&T::version>,
        "create_revision", quoted_num<&T::create_revision>,
        "mod_revision", quoted_num<&T::mod_revision>,
        "value", OptionalBase64String<&T::value>,
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
struct glz::meta<EtcdCompactionRequest> {
    using T = EtcdCompactionRequest;
    static constexpr auto value = object(
        "revision", quoted_num<&T::revision>,
        "physical", &T::physical
    );
};

template <>
struct glz::meta<EtcdCompactionResponse> {
    using T = EtcdCompactionResponse;
    static constexpr auto value = object(
        "header", &T::header
    );
};

template <>
struct glz::meta<LeaseGrantRequest> {
    using T = LeaseGrantRequest;
    static constexpr auto value = object(
        "TTL", quoted_num<&T::ttl_in_seconds>,
        "ID", quoted_num<&T::id>
    );
};

template <>
struct glz::meta<LeaseGrantResponse> {
    using T = LeaseGrantResponse;
    static constexpr auto value = object(
        "header", &T::header,
        "TTL", quoted_num<&T::ttl_in_seconds>,
        "ID", quoted_num<&T::id>,
        "error", OptionalBase64String<&T::error>
    );
};

template <>
struct glz::meta<LeaseRevokeRequest> {
    using T = LeaseRevokeRequest;
    static constexpr auto value = object(
        "ID", quoted_num<&T::id>
    );
};

template <>
struct glz::meta<LeaseRevokeResponse> {
    using T = LeaseRevokeResponse;
    static constexpr auto value = object(
        "header", &T::header
    );
};

template <>
struct glz::meta<LeaseKeepAliveRequest> {
    using T = LeaseKeepAliveRequest;
    static constexpr auto value = object(
        "ID", quoted_num<&T::id>
    );
};

template <>
struct glz::meta<LeaseKeepAliveWrapper> {
    using T = LeaseKeepAliveWrapper;
    static constexpr auto value = object(
        "header", &T::header,
        "TTL", quoted_num<&T::ttl_in_seconds>,
        "ID", quoted_num<&T::id>
    );
};

template <>
struct glz::meta<LeaseKeepAliveResponse> {
    using T = LeaseKeepAliveResponse;
    static constexpr auto value = object(
        "result", &T::result
    );
};

template <>
struct glz::meta<LeaseTimeToLiveRequest> {
    using T = LeaseTimeToLiveRequest;
    static constexpr auto value = object(
        "ID", quoted_num<&T::id>,
        "keys", &T::keys
    );
};

template <>
struct glz::meta<LeaseTimeToLiveResponse> {
    using T = LeaseTimeToLiveResponse;
    static constexpr auto value = object(
        "header", &T::header,
        "TTL", quoted_num<&T::ttl_in_seconds>,
        "ID", quoted_num<&T::id>,
        "grantedTTL", quoted_num<&T::granted_ttl>,
        "keys", VectorBase64String<&T::keys>
    );
};

template <>
struct glz::meta<LeaseLeasesRequest> {
    using T = LeaseLeasesRequest;
    static constexpr auto value = object(
    );
};

template <>
struct glz::meta<LeaseStatus> {
    using T = LeaseStatus;
    static constexpr auto value = object(
            "ID", quoted_num<&T::id>
    );
};

template <>
struct glz::meta<LeaseLeasesResponse> {
    using T = LeaseLeasesResponse;
    static constexpr auto value = object(
            "header", &T::header,
            "leases", &T::leases
    );
};

template <>
struct glz::meta<AuthEnableRequest> {
    using T = AuthEnableRequest;
    static constexpr auto value = object(
    );
};

template <>
struct glz::meta<AuthEnableResponse> {
    using T = AuthEnableResponse;
    static constexpr auto value = object(
            "header", &T::header
    );
};

template <>
struct glz::meta<AuthDisableRequest> {
    using T = AuthDisableRequest;
    static constexpr auto value = object(
    );
};

template <>
struct glz::meta<AuthDisableResponse> {
    using T = AuthDisableResponse;
    static constexpr auto value = object(
            "header", &T::header
    );
};

template <>
struct glz::meta<AuthStatusRequest> {
    using T = AuthStatusRequest;
    static constexpr auto value = object(
    );
};

template <>
struct glz::meta<AuthStatusResponse> {
    using T = AuthStatusResponse;
    static constexpr auto value = object(
            "header", &T::header,
            "enabled", &T::enabled,
            "authRevision", quoted_num<&T::authRevision>
    );
};

template <>
struct glz::meta<AuthenticateRequest> {
    using T = AuthenticateRequest;
    static constexpr auto value = object(
            "name", &T::name,
            "password", &T::password
    );
};

template <>
struct glz::meta<AuthenticateResponse> {
    using T = AuthenticateResponse;
    static constexpr auto value = object(
            "header", &T::header,
            "token", &T::token
    );
};

template <>
struct glz::meta<AuthUserAddOptions> {
    using T = AuthUserAddOptions;
    static constexpr auto value = object(
            "no_password", &T::no_password
    );
};

template <>
struct glz::meta<AuthPermission> {
    using T = AuthPermission;
    static constexpr auto value = object(
            "permType", StringEnum<&T::permType>,
            "key", Base64String<&T::key>,
            "range_end", Base64String<&T::range_end>
    );
};

template <>
struct glz::meta<AuthUserAddRequest> {
    using T = AuthUserAddRequest;
    static constexpr auto value = object(
            "name", &T::name,
            "password", &T::password,
            "options", Optional<&T::options>,
            "hashedPassword", Optional<&T::hashedPassword>
    );
};

template <>
struct glz::meta<AuthUserAddResponse> {
    using T = AuthUserAddResponse;
    static constexpr auto value = object(
            "header", &T::header
    );
};

template <>
struct glz::meta<AuthUserGetRequest> {
    using T = AuthUserGetRequest;
    static constexpr auto value = object(
            "name", &T::name
    );
};

template <>
struct glz::meta<AuthUserGetResponse> {
    using T = AuthUserGetResponse;
    static constexpr auto value = object(
            "header", &T::header,
            "roles", &T::roles
    );
};

template <>
struct glz::meta<AuthUserListRequest> {
    using T = AuthUserListRequest;
    static constexpr auto value = object(
    );
};

template <>
struct glz::meta<AuthUserListResponse> {
    using T = AuthUserListResponse;
    static constexpr auto value = object(
            "header", &T::header,
            "users", &T::users
    );
};

template <>
struct glz::meta<AuthUserDeleteRequest> {
    using T = AuthUserDeleteRequest;
    static constexpr auto value = object(
            "name", &T::name
    );
};

template <>
struct glz::meta<AuthUserDeleteResponse> {
    using T = AuthUserDeleteResponse;
    static constexpr auto value = object(
            "header", &T::header
    );
};

template <>
struct glz::meta<AuthUserChangePasswordRequest> {
    using T = AuthUserChangePasswordRequest;
    static constexpr auto value = object(
            "name", &T::name,
            "password", &T::password,
            "hashedPassword", Optional<&T::hashedPassword>
    );
};

template <>
struct glz::meta<AuthUserChangePasswordResponse> {
    using T = AuthUserChangePasswordResponse;
    static constexpr auto value = object(
            "header", &T::header
    );
};

template <>
struct glz::meta<AuthUserGrantRoleRequest> {
    using T = AuthUserGrantRoleRequest;
    static constexpr auto value = object(
            "user", &T::user,
            "role", &T::role
    );
};

template <>
struct glz::meta<AuthUserGrantRoleResponse> {
    using T = AuthUserGrantRoleResponse;
    static constexpr auto value = object(
            "header", &T::header
    );
};

template <>
struct glz::meta<AuthUserRevokeRoleRequest> {
    using T = AuthUserRevokeRoleRequest;
    static constexpr auto value = object(
            "name", &T::name,
            "role", &T::role
    );
};

template <>
struct glz::meta<AuthUserRevokeRoleResponse> {
    using T = AuthUserRevokeRoleResponse;
    static constexpr auto value = object(
            "header", &T::header
    );
};

template <>
struct glz::meta<AuthRoleAddRequest> {
    using T = AuthRoleAddRequest;
    static constexpr auto value = object(
            "name", &T::name
    );
};

template <>
struct glz::meta<AuthRoleAddResponse> {
    using T = AuthRoleAddResponse;
    static constexpr auto value = object(
            "header", &T::header
    );
};

template <>
struct glz::meta<AuthRoleGetRequest> {
    using T = AuthRoleGetRequest;
    static constexpr auto value = object(
            "role", &T::role
    );
};

template <>
struct glz::meta<AuthRoleGetResponse> {
    using T = AuthRoleGetResponse;
    static constexpr auto value = object(
            "header", &T::header,
            "perm", &T::perm
    );
};

template <>
struct glz::meta<AuthRoleListRequest> {
    using T = AuthRoleListRequest;
    static constexpr auto value = object(
    );
};

template <>
struct glz::meta<AuthRoleListResponse> {
    using T = AuthRoleListResponse;
    static constexpr auto value = object(
            "header", &T::header,
            "roles", &T::roles
    );
};

template <>
struct glz::meta<AuthRoleDeleteRequest> {
    using T = AuthRoleDeleteRequest;
    static constexpr auto value = object(
            "role", &T::role
    );
};

template <>
struct glz::meta<AuthRoleDeleteResponse> {
    using T = AuthRoleDeleteResponse;
    static constexpr auto value = object(
            "header", &T::header
    );
};

template <>
struct glz::meta<AuthRoleGrantPermissionRequest> {
    using T = AuthRoleGrantPermissionRequest;
    static constexpr auto value = object(
            "name", &T::name,
            "perm", &T::perm
    );
};

template <>
struct glz::meta<AuthRoleGrantPermissionResponse> {
    using T = AuthRoleGrantPermissionResponse;
    static constexpr auto value = object(
            "header", &T::header
    );
};

template <>
struct glz::meta<AuthRoleRevokePermissionRequest> {
    using T = AuthRoleRevokePermissionRequest;
    static constexpr auto value = object(
            "role", &T::role,
            "key", Base64String<&T::key>,
            "range_end", OptionalBase64String<&T::range_end>
    );
};

template <>
struct glz::meta<AuthRoleRevokePermissionRequestBugWorkaround> {
    using T = AuthRoleRevokePermissionRequestBugWorkaround;
    static constexpr auto value = object(
            "role", &T::role,
            "key", &T::key,
            "range_end", Optional<&T::range_end>
    );
};

template <>
struct glz::meta<AuthRoleRevokePermissionResponse> {
    using T = AuthRoleRevokePermissionResponse;
    static constexpr auto value = object(
            "header", &T::header
    );
};

template <>
struct glz::meta<EtcdWatchCreateRequest> {
    using T = EtcdWatchCreateRequest;
    static constexpr auto value = object(
            "key", Base64String<&T::key>,
            "range_end", OptionalBase64String<&T::range_end>,
            "start_revision", quoted_num<&T::start_revision>,
            "progress_notify", &T::progress_notify,
            "prev_kv", &T::prev_kv,
            "watch_id", quoted_num<&T::watch_id>,
            "fragment", &T::fragment
    );
};

template <>
struct glz::meta<EtcdWatchResponse> {
    using T = EtcdWatchResponse;
    static constexpr auto value = object(
            "header", &T::header
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
        "etcdcluster", &T::etcdcluster,
        "storage", &T::storage
    );
};

namespace glz::detail {
    template <>
    struct from_json<tl::optional<EtcdKeyValue>> {
        template <auto Opts>
        static void op(tl::optional<EtcdKeyValue>& value, auto&&... args) {
            EtcdKeyValue val;
            read<json>::op<Opts>(val, args...);
            value = val;
        }
    };

    template <>
    struct to_json<tl::optional<EtcdKeyValue>> {
        template <auto Opts>
        static void op(tl::optional<EtcdKeyValue> const & value, auto&&... args) noexcept {
            if(value) {
                write<json>::op<Opts>(*value, args...);
            }
        }
    };

    template <>
    struct from_json<tl::optional<EtcdRangeResponse>> {
        template <auto Opts>
        static void op(tl::optional<EtcdRangeResponse>& value, auto&&... args) {
            fmt::print("EtcdRangeResponse read\n");
            EtcdRangeResponse val;
            read<json>::op<Opts>(val, args...);
            value = val;
        }
    };

    template <>
    struct to_json<tl::optional<EtcdRangeResponse>> {
        template <auto Opts>
        static void op(tl::optional<EtcdRangeResponse> const & value, auto&&... args) noexcept {
            fmt::print("EtcdRangeResponse write\n");
            if(value) {
                write<json>::op<Opts>(*value, args...);
            }
        }
    };

    template <>
    struct from_json<tl::optional<EtcdPutResponse>> {
        template <auto Opts>
        static void op(tl::optional<EtcdPutResponse>& value, auto&&... args) {
            EtcdPutResponse val;
            read<json>::op<Opts>(val, args...);
            value = val;
        }
    };

    template <>
    struct to_json<tl::optional<EtcdPutResponse>> {
        template <auto Opts>
        static void op(tl::optional<EtcdPutResponse> const & value, auto&&... args) noexcept {
            if(value) {
                write<json>::op<Opts>(*value, args...);
            }
        }
    };

    template <>
    struct from_json<tl::optional<EtcdDeleteRangeResponse>> {
        template <auto Opts>
        static void op(tl::optional<EtcdDeleteRangeResponse>& value, auto&&... args) {
            EtcdDeleteRangeResponse val;
            read<json>::op<Opts>(val, args...);
            value = val;
        }
    };

    template <>
    struct to_json<tl::optional<EtcdDeleteRangeResponse>> {
        template <auto Opts>
        static void op(tl::optional<EtcdDeleteRangeResponse> const & value, auto&&... args) noexcept {
            if(value) {
                write<json>::op<Opts>(*value, args...);
            }
        }
    };

    template <>
    struct from_json<tl::optional<EtcdTxnResponse>> {
        template <auto Opts>
        static void op(tl::optional<EtcdTxnResponse>& value, auto&&... args) {
            EtcdTxnResponse val;
            read<json>::op<Opts>(val, args...);
            value = val;
        }
    };

    template <>
    struct to_json<tl::optional<EtcdTxnResponse>> {
        template <auto Opts>
        static void op(tl::optional<EtcdTxnResponse> const & value, auto&&... args) noexcept {
            if(value) {
                write<json>::op<Opts>(*value, args...);
            }
        }
    };
}

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

    ICHOR_LOG_TRACE(_logger, "Started, detected server version {}", (*ver).etcdserver);
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
    ICHOR_LOG_TRACE(logger, "{} status: {}, body: {}", url, (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(logger, "Error on route {}, http status {}, body {}", url, (int)http_reply.status, http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(EtcdError::HTTP_RESPONSE_ERROR);
    }

    RespT etcd_reply;
    auto err = glz::template read<glz::opts{.error_on_const_read = true}, RespT>(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(logger, "json {}", http_reply.body.empty() ? "" : http_reply.body.empty() ? "" : (char*)http_reply.body.data());
        co_return tl::unexpected(EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<EtcdPutResponse, EtcdError>> EtcdService::put(EtcdPutRequest const &req) {
    if(req.prev_kv && _detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request prevKv for etcd server for etcdserver version {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }
    if(req.ignore_value && _detectedVersion < Version{3, 2, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request ignoreValue for etcd server for etcdserver version {}, minimum 3.2.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }
    if(req.ignore_lease && _detectedVersion < Version{3, 2, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request ignoreLease for etcd server for etcdserver version {}, minimum 3.2.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    co_return co_await execute_request<EtcdPutRequest, EtcdPutResponse>(fmt::format("{}/kv/put", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<EtcdRangeResponse, EtcdError>> EtcdService::range(EtcdRangeRequest const &req) {
    if(req.min_mod_revision && _detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request min_mod_revision for etcd server for etcdserver version {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }
    if(req.max_mod_revision && _detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request max_mod_revision for etcd server for etcdserver version {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }
    if(req.min_create_revision && _detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request min_create_revision for etcd server for etcdserver version {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }
    if(req.max_create_revision && _detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request max_create_revision for etcd server for etcdserver version {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    co_return co_await execute_request<EtcdRangeRequest, EtcdRangeResponse>(fmt::format("{}/kv/range", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<EtcdDeleteRangeResponse, EtcdError>> EtcdService::deleteRange(EtcdDeleteRangeRequest const &req) {
    if(req.prev_kv && _detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request prev_kv for etcd server for etcdserver version {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    co_return co_await execute_request<EtcdDeleteRangeRequest, EtcdDeleteRangeResponse>(fmt::format("{}/kv/deleterange", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<EtcdTxnResponse, EtcdError>> EtcdService::txn(EtcdTxnRequest const &req) {
    for(auto const &comp : req.compare) {
        if(comp.result == EtcdCompareResult::NOT_EQUAL && _detectedVersion < Version{3, 1, 0}) {
            ICHOR_LOG_ERROR(_logger, "Cannot request EtcdCompareResult::NOT_EQUAL for etcd server for etcdserver version {}, minimum 3.1.0 required", _detectedVersion);
            co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
        }
        if(comp.target == EtcdCompareTarget::LEASE && _detectedVersion < Version{3, 3, 0}) {
            ICHOR_LOG_ERROR(_logger, "Cannot request EtcdCompareTarget::LEASE for etcd server for etcdserver version {}, minimum 3.3.0 required", _detectedVersion);
            co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
        }
        if(comp.lease && _detectedVersion < Version{3, 3, 0}) {
            ICHOR_LOG_ERROR(_logger, "Cannot request lease for etcd server for etcdserver version {}, minimum 3.3.0 required", _detectedVersion);
            co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
        }
        if(comp.range_end && _detectedVersion < Version{3, 3, 0}) {
            ICHOR_LOG_ERROR(_logger, "Cannot request range_end for etcd server for etcdserver version {}, minimum 3.3.0 required", _detectedVersion);
            co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
        }
    }
    for(auto const &op : req.success) {
        if(op.request_txn && _detectedVersion < Version{3, 3, 0}) {
            ICHOR_LOG_ERROR(_logger, "Cannot request request_txn for etcd server for etcdserver version {}, minimum 3.3.0 required", _detectedVersion);
            co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
        }
    }
    for(auto const &op : req.failure) {
        if(op.request_txn && _detectedVersion < Version{3, 3, 0}) {
            ICHOR_LOG_ERROR(_logger, "Cannot request request_txn for etcd server for etcdserver version {}, minimum 3.3.0 required", _detectedVersion);
            co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
        }
    }

    co_return co_await execute_request<EtcdTxnRequest, EtcdTxnResponse>(fmt::format("{}/kv/txn", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<EtcdCompactionResponse, EtcdError>> EtcdService::compact(EtcdCompactionRequest const &req) {
    co_return co_await execute_request<EtcdCompactionRequest, EtcdCompactionResponse>(fmt::format("{}/kv/compaction", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<LeaseGrantResponse, EtcdError>> EtcdService::leaseGrant(LeaseGrantRequest const &req) {
    // for some reason this one that doesn't need kv/lease/...
    co_return co_await execute_request<LeaseGrantRequest, LeaseGrantResponse>(fmt::format("{}/lease/grant", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<LeaseRevokeResponse, EtcdError>> EtcdService::leaseRevoke(LeaseRevokeRequest const &req) {
    // older versions use kv/lease/..., newer versions support this but default to /lease/.... Something to watch out for.
    co_return co_await execute_request<LeaseRevokeRequest, LeaseRevokeResponse>(fmt::format("{}/kv/lease/revoke", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<LeaseKeepAliveResponse, EtcdError>> EtcdService::leaseKeepAlive(LeaseKeepAliveRequest const &req) {
    if(_detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot use leaseKeepAlive for etcdserver version {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    // for some reason this one that doesn't need kv/lease/...
    co_return co_await execute_request<LeaseKeepAliveRequest, LeaseKeepAliveResponse>(fmt::format("{}/lease/keepalive", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<LeaseTimeToLiveResponse, EtcdError>> EtcdService::leaseTimeToLive(LeaseTimeToLiveRequest const &req) {
    if(_detectedVersion < Version{3, 1, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot use leaseTimeToLive for etcdserver version {}, minimum 3.1.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    // older versions use kv/lease/..., newer versions support this but default to /lease/.... Something to watch out for.
    co_return co_await execute_request<LeaseTimeToLiveRequest, LeaseTimeToLiveResponse>(fmt::format("{}/kv/lease/timetolive", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<LeaseLeasesResponse, EtcdError>> EtcdService::leaseLeases(LeaseLeasesRequest const &req) {
    if(_detectedVersion < Version{3, 3, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot use leaseLeases for etcdserver version {}, minimum 3.3.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    // older versions use kv/lease/..., newer versions support this but default to /lease/.... Something to watch out for.
    co_return co_await execute_request<LeaseLeasesRequest, LeaseLeasesResponse>(fmt::format("{}/kv/lease/leases", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthEnableResponse, EtcdError>> EtcdService::authEnable(AuthEnableRequest const &req) {
    if(_detectedVersion < Version{3, 3, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot use authEnable for etcdserver version {}, minimum 3.3.0 required, see https://github.com/etcd-io/etcd/issues/6643", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    co_return co_await execute_request<AuthEnableRequest, AuthEnableResponse>(fmt::format("{}/auth/enable", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthDisableResponse, EtcdError>> EtcdService::authDisable(AuthDisableRequest const &req) {
    if(_detectedVersion < Version{3, 3, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot use authDisable for etcdserver version {}, minimum 3.3.0 required, see https://github.com/etcd-io/etcd/issues/6643", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    auto resp = co_await execute_request<AuthDisableRequest, AuthDisableResponse>(fmt::format("{}/auth/disable", _versionSpecificUrl), _auth, _logger, _mainConn, req);
    if(resp) {
        _auth.reset();
        _authUser.reset();
    }
    co_return resp;
}

Ichor::Task<tl::expected<AuthStatusResponse, EtcdError>> EtcdService::authStatus(AuthStatusRequest const &req) {
    if(_detectedVersion < Version{3, 5, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot use AuthStatus for etcdserver version {}, minimum 3.5.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    co_return co_await execute_request<AuthStatusRequest, AuthStatusResponse>(fmt::format("{}/auth/status", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthenticateResponse, EtcdError>> EtcdService::authenticate(AuthenticateRequest const &req) {
    if(_detectedVersion < Version{3, 3, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot use authenticate for etcdserver version {}, minimum 3.3.0 required, see https://github.com/etcd-io/etcd/issues/6643", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    tl::expected<AuthenticateResponse, EtcdError> resp = co_await execute_request<AuthenticateRequest, AuthenticateResponse>(fmt::format("{}/auth/authenticate", _versionSpecificUrl), _auth, _logger, _mainConn, req);
    if(resp) {
        _auth = resp->token;
        _authUser = req.name;
    }
    co_return resp;
}

Ichor::Task<tl::expected<AuthUserAddResponse, EtcdError>> EtcdService::userAdd(AuthUserAddRequest const &req) {
    if(req.options && _detectedVersion < Version{3, 4, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request options for etcd server for etcdserver version {}, minimum 3.4.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }
    if(req.hashedPassword && _detectedVersion < Version{3, 5, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request hashedPassword for etcd server for etcdserver version {}, minimum 3.5.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    co_return co_await execute_request<AuthUserAddRequest, AuthUserAddResponse>(fmt::format("{}/auth/user/add", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthUserGetResponse, EtcdError>> EtcdService::userGet(AuthUserGetRequest const &req) {
    co_return co_await execute_request<AuthUserGetRequest, AuthUserGetResponse>(fmt::format("{}/auth/user/get", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthUserListResponse, EtcdError>> EtcdService::userList(AuthUserListRequest const &req) {
    co_return co_await execute_request<AuthUserListRequest, AuthUserListResponse>(fmt::format("{}/auth/user/list", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthUserDeleteResponse, EtcdError>> EtcdService::userDelete(AuthUserDeleteRequest const &req) {
    co_return co_await execute_request<AuthUserDeleteRequest, AuthUserDeleteResponse>(fmt::format("{}/auth/user/delete", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthUserChangePasswordResponse, EtcdError>> EtcdService::userChangePassword(AuthUserChangePasswordRequest const &req) {
    if(req.hashedPassword && _detectedVersion < Version{3, 5, 0}) {
        ICHOR_LOG_ERROR(_logger, "Cannot request hashedPassword for etcd server for etcdserver version {}, minimum 3.5.0 required", _detectedVersion);
        co_return tl::unexpected(EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT);
    }

    co_return co_await execute_request<AuthUserChangePasswordRequest, AuthUserChangePasswordResponse>(fmt::format("{}/auth/user/changepw", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthUserGrantRoleResponse, EtcdError>> EtcdService::userGrantRole(AuthUserGrantRoleRequest const &req) {
    co_return co_await execute_request<AuthUserGrantRoleRequest, AuthUserGrantRoleResponse>(fmt::format("{}/auth/user/grant", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthUserRevokeRoleResponse, EtcdError>> EtcdService::userRevokeRole(AuthUserRevokeRoleRequest const &req) {
    co_return co_await execute_request<AuthUserRevokeRoleRequest, AuthUserRevokeRoleResponse>(fmt::format("{}/auth/user/revoke", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthRoleAddResponse, EtcdError>> EtcdService::roleAdd(AuthRoleAddRequest const &req) {
    co_return co_await execute_request<AuthRoleAddRequest, AuthRoleAddResponse>(fmt::format("{}/auth/role/add", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthRoleGetResponse, EtcdError>> EtcdService::roleGet(AuthRoleGetRequest const &req) {
    co_return co_await execute_request<AuthRoleGetRequest, AuthRoleGetResponse>(fmt::format("{}/auth/role/get", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthRoleListResponse, EtcdError>> EtcdService::roleList(AuthRoleListRequest const &req) {
    co_return co_await execute_request<AuthRoleListRequest, AuthRoleListResponse>(fmt::format("{}/auth/role/list", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthRoleDeleteResponse, EtcdError>> EtcdService::roleDelete(AuthRoleDeleteRequest const &req) {
    co_return co_await execute_request<AuthRoleDeleteRequest, AuthRoleDeleteResponse>(fmt::format("{}/auth/role/delete", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthRoleGrantPermissionResponse, EtcdError>> EtcdService::roleGrantPermission(AuthRoleGrantPermissionRequest const &req) {
    co_return co_await execute_request<AuthRoleGrantPermissionRequest, AuthRoleGrantPermissionResponse>(fmt::format("{}/auth/role/grant", _versionSpecificUrl), _auth, _logger, _mainConn, req);
}

Ichor::Task<tl::expected<AuthRoleRevokePermissionResponse, EtcdError>> EtcdService::roleRevokePermission(AuthRoleRevokePermissionRequest const &req) {
    if(_detectedVersion < Version{3, 4, 0}) {
        ICHOR_LOG_WARN(_logger, "Etcd version 3.3.X and lower contain a bug regarding parsing Key/RangeEnd. Results might not be what you expect due to not supporting UTF-8 nor binary. See https://github.com/etcd-io/etcd/issues/9424 for more information.");
        co_return co_await execute_request<AuthRoleRevokePermissionRequestBugWorkaround, AuthRoleRevokePermissionResponse>(fmt::format("{}/auth/role/revoke", _versionSpecificUrl), _auth, _logger, _mainConn, AuthRoleRevokePermissionRequestBugWorkaround{req});
    } else {
        co_return co_await execute_request<AuthRoleRevokePermissionRequest, AuthRoleRevokePermissionResponse>(fmt::format("{}/auth/role/revoke", _versionSpecificUrl), _auth, _logger, _mainConn, req);
    }
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

    std::string_view etcdserver_version = etcd_reply.etcdserver;
    if(auto loc = etcdserver_version.find('-'); loc != std::string_view::npos) {
        etcdserver_version = etcdserver_version.substr(0, loc);
    }

    auto etcdserverver = parseStringAsVersion(etcdserver_version);

    if(!etcdserverver) {
        ICHOR_LOG_ERROR(_logger, "Error parsing etcd server version \"{}\"", etcd_reply.etcdserver);
        co_return tl::unexpected(EtcdError::VERSION_PARSE_ERROR);
    }

    auto etcdclusterver = parseStringAsVersion(etcd_reply.etcdcluster);

    if(!etcdclusterver) {
        ICHOR_LOG_ERROR(_logger, "Error parsing etcd server version \"{}\"", etcd_reply.etcdcluster);
        co_return tl::unexpected(EtcdError::VERSION_PARSE_ERROR);
    }

    tl::optional<Version> etcdstoragever;
    if(etcd_reply.storage) {
        etcdstoragever = parseStringAsVersion(*etcd_reply.storage);

        if(!etcdstoragever) {
            ICHOR_LOG_ERROR(_logger, "Error parsing etcd server version \"{}\"", *etcd_reply.storage);
            co_return tl::unexpected(EtcdError::VERSION_PARSE_ERROR);
        }
    }

    co_return EtcdVersionReply{*etcdserverver, *etcdclusterver, etcdstoragever};
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

tl::optional<std::string> const &EtcdService::getAuthenticationUser() const {
    return _authUser;
}
