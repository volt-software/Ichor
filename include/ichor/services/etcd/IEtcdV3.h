#pragma once

#include <ichor/coroutines/Task.h>
#include <ichor/stl/StringUtils.h>
#include <string_view>
#include <tl/optional.h>
#include <string>
#include <tl/expected.h>
#include <fmt/core.h>

namespace Ichor::Etcd::v3 {
    enum class EtcdError : uint_fast16_t {
        HTTP_RESPONSE_ERROR,
        VERSION_PARSE_ERROR,
        JSON_PARSE_ERROR,
        TIMEOUT,
        CONNECTION_CLOSED_PREMATURELY_TRY_AGAIN,
        ROOT_USER_NOT_YET_CREATED,
        REMOVING_ROOT_NOT_ALLOWED_WITH_AUTH_ENABLED,
        NO_AUTHENTICATION_SET,
        UNAUTHORIZED,
        NOT_FOUND,
        DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT,
        CANNOT_DELETE_ROOT_WHILE_AUTH_IS_ENABLED,
        QUITTING,
        ETCD_SERVER_DOES_NOT_SUPPORT
    };

    enum class EtcdEventType : uint_fast16_t {
        PUT = 0,
        DELETE_ = 1
    };

    enum class EtcdSortOrder : uint_fast16_t {
        NONE = 0,
        ASCEND = 1,
        DESCEND = 2
    };

    enum class EtcdSortTarget : uint_fast16_t {
        KEY = 0,
        VERSION = 1,
        CREATE = 2,
        MOD = 3,
        VALUE = 4
    };

    enum class EtcdCompareResult : uint_fast16_t {
        EQUAL = 0,
        GREATER = 1,
        LESS = 2,
        NOT_EQUAL = 3
    };

    enum class EtcdCompareTarget : uint_fast16_t {
        VERSION = 0,
        CREATE = 1,
        MOD = 2,
        VALUE = 3,
        LEASE = 4
    };

    enum class EtcdAuthPermissionType : uint_fast16_t {
        READ = 0,
        WRITE = 1,
        READWRITE = 2
    };

    struct EtcdKeyValue final {
        std::string key;
        std::string value;
        int64_t create_revision{};
        int64_t mod_revision{};
        int64_t version{};
        int64_t lease{};
    };

    struct EtcdEvent final {
        EtcdEventType type;
        EtcdKeyValue kv;
        EtcdKeyValue prev_kv;
    };

    struct EtcdResponseHeader final {
        uint64_t cluster_id{};
        uint64_t member_id{};
        int64_t revision{};
        uint64_t raft_term{};
    };

    struct EtcdRangeRequest final {
        std::string key;
        tl::optional<std::string> range_end;
        tl::optional<int64_t> limit{};
        tl::optional<int64_t> revision{};
        tl::optional<EtcdSortOrder> sort_order{};
        tl::optional<EtcdSortTarget> sort_target{};
        tl::optional<bool> serializable{};
        tl::optional<bool> keys_only{};
        tl::optional<bool> count_only{};
        tl::optional<int64_t> min_mod_revision;
        tl::optional<int64_t> max_mod_revision;
        tl::optional<int64_t> min_create_revision;
        tl::optional<int64_t> max_create_revision;
    };

    struct EtcdRangeResponse final {
        EtcdResponseHeader header;
        std::vector<EtcdKeyValue> kvs;
        int64_t count{};
        tl::optional<bool> more{};
    };

    struct EtcdPutRequest final {
        std::string key;
        std::string value;
        tl::optional<int64_t> lease{};
        tl::optional<bool> prev_kv;
        tl::optional<bool> ignore_value;
        tl::optional<bool> ignore_lease;
    };

    struct EtcdPutResponse final {
        EtcdResponseHeader header;
        tl::optional<EtcdKeyValue> prev_kv;
    };

    struct EtcdDeleteRangeRequest final {
        std::string key;
        tl::optional<std::string> range_end;
        tl::optional<bool> prev_kv;

    };

    struct EtcdDeleteRangeResponse final {
        EtcdResponseHeader header;
        int64_t deleted{};
        std::vector<EtcdKeyValue> prev_kvs;

    };

    struct EtcdCompare final {
        EtcdCompareResult result{};
        EtcdCompareTarget target{};
        std::string key;
        tl::optional<std::string> range_end;
        // one of:
        // {
        tl::optional<int64_t> version;
        tl::optional<int64_t> create_revision;
        tl::optional<int64_t> mod_revision;
        tl::optional<std::string> value;
        tl::optional<int64_t> lease;
        // }
    };

    struct EtcdRequestOp;
    struct EtcdResponseOp;

    struct EtcdTxnRequest final {
        std::vector<EtcdCompare> compare;
        std::vector<EtcdRequestOp> success;
        std::vector<EtcdRequestOp> failure;
    };

    struct EtcdTxnResponse final {
        EtcdResponseHeader header;
        bool succeeded;
        std::vector<EtcdResponseOp> responses;
    };

    struct EtcdRequestOp final {
        // one of:
        // {
        tl::optional<EtcdRangeRequest> request_range;
        tl::optional<EtcdPutRequest> request_put;
        tl::optional<EtcdDeleteRangeRequest> request_delete;
        tl::optional<EtcdTxnRequest> request_txn;
        // }
    };

    struct EtcdResponseOp final {
        // one of:
        // {
        tl::optional<EtcdRangeResponse> response_range;
        tl::optional<EtcdPutResponse> response_put;
        tl::optional<EtcdDeleteRangeResponse> response_delete_range;
        tl::optional<EtcdTxnResponse> response_txn;
        // }
    };

    struct EtcdCompactionRequest final {
        int64_t revision;
        tl::optional<bool> physical;
    };

    struct EtcdCompactionResponse final {
        EtcdResponseHeader header;
    };

    struct LeaseGrantRequest final {
        int64_t ttl_in_seconds;
        int64_t id;
    };

    struct LeaseGrantResponse final {
        EtcdResponseHeader header;
        int64_t id;
        int64_t ttl_in_seconds;
        tl::optional<std::string> error;
    };

    struct LeaseRevokeRequest final {
        int64_t id;
    };

    struct LeaseRevokeResponse final {
        EtcdResponseHeader header;
    };

    struct LeaseKeepAliveRequest final {
        int64_t id;
    };

    struct LeaseKeepAliveWrapper final {
        EtcdResponseHeader header;
        int64_t id;
        int64_t ttl_in_seconds;
    };

    struct LeaseKeepAliveResponse final {
        LeaseKeepAliveWrapper result;
    };

    struct LeaseTimeToLiveRequest final {
        int64_t id;
        bool keys;
    };

    struct LeaseTimeToLiveResponse final {
        EtcdResponseHeader header;
        int64_t id;
        int64_t ttl_in_seconds;
        int64_t granted_ttl;
        std::vector<std::string> keys;
    };

    struct LeaseLeasesRequest final {
    };

    struct LeaseStatus final {
        int64_t id;
    };

    struct LeaseLeasesResponse final {
        EtcdResponseHeader header;
        std::vector<LeaseStatus> leases;
    };

    struct AuthEnableRequest final {
    };

    struct AuthEnableResponse final {
        EtcdResponseHeader header;
    };

    struct AuthDisableRequest final {
    };

    struct AuthDisableResponse final {
        EtcdResponseHeader header;
    };

    struct AuthStatusRequest final {
    };

    struct AuthStatusResponse final {
        EtcdResponseHeader header;
        bool enabled;
        uint64_t authRevision;
    };

    struct AuthenticateRequest final {
        std::string name;
        std::string password;
    };

    struct AuthenticateResponse final {
        EtcdResponseHeader header;
        std::string token;
    };

    struct AuthUserAddOptions final {
        bool no_password;
    };

    struct AuthPermission final {
        EtcdAuthPermissionType permType;
        std::string key;
        std::string range_end;
    };

    struct AuthRole final {
        std::string name;
        std::vector<AuthPermission> keyPermission;
    };

    struct AuthUserAddRequest final {
        std::string name;
        std::string password;
        AuthUserAddOptions options;
        std::string hashedPassword;
    };

    struct AuthUserAddResponse final {
        EtcdResponseHeader header;
    };

    struct AuthUserGetRequest final {
        std::string name;
    };

    struct AuthUserGetResponse final {
        EtcdResponseHeader header;
        std::vector<std::string> roles;
    };

    struct AuthUserListRequest final {
    };

    struct AuthUserListResponse final {
        EtcdResponseHeader header;
        std::vector<std::string> users;
    };

    struct AuthUserDeleteRequest final {
        std::string name;
    };

    struct AuthUserDeleteResponse final {
        EtcdResponseHeader header;
    };

    struct AuthUserChangePasswordRequest final {
        std::string name;
        std::string password;
        std::string hashedPassword;
    };

    struct AuthUserChangePasswordResponse final {
        EtcdResponseHeader header;
    };

    struct AuthUserGrantRoleRequest final {
        std::string name;
        std::string role;
    };

    struct AuthUserGrantRoleResponse final {
        EtcdResponseHeader header;
    };

    struct AuthUserRevokeRoleRequest final {
        std::string name;
        std::string role;
    };

    struct AuthUserRevokeRoleResponse final {
        EtcdResponseHeader header;
    };

    struct AuthRoleAddRequest final {
        std::string name;
    };

    struct AuthRoleAddResponse final {
        EtcdResponseHeader header;
    };

    struct AuthRoleGetRequest final {
        std::string role;
    };

    struct AuthRoleGetResponse final {
        EtcdResponseHeader header;
        std::vector<AuthPermission> perm;
    };

    struct AuthRoleListRequest final {
    };

    struct AuthRoleListResponse final {
        EtcdResponseHeader header;
        std::vector<std::string> roles;
    };

    struct AuthRoleDeleteRequest final {
        std::string role;
    };

    struct AuthRoleDeleteResponse final {
        EtcdResponseHeader header;
    };

    struct AuthRoleGrantPermissionRequest final {
        std::string name;
        AuthPermission perm;
    };

    struct AuthRoleGrantPermissionResponse final {
        EtcdResponseHeader header;
    };

    struct AuthRoleRevokePermissionRequest final {
        std::string name;
        std::string key;
        std::string range_end;
    };

    struct AuthRoleRevokePermissionResponse final {
        EtcdResponseHeader header;
    };

    struct EtcdWatchRequest final {

    };

    struct EtcdWatchResponse final {
        EtcdResponseHeader header;
    };

    struct EtcdVersionReply final {
        Version etcdserver;
        Version etcdcluster;
    };

    class IEtcd {
    public:
        /**
         * Create or update or compare-and-swap operation
         *
         * @param req
         * @return Either the EtcdPutResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdPutResponse, EtcdError>> put(EtcdPutRequest const &req) = 0;

        /**
         * Gets (multiple) key(s)
         *
         * @param req
         * @return Either the EtcdRangeResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdRangeResponse, EtcdError>> range(EtcdRangeRequest const &req) = 0;

        /**
         * Delete (multiple) key(s)
         *
         * @param req
         * @return Either the EtcdRangeResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdDeleteRangeResponse, EtcdError>> deleteRange(EtcdDeleteRangeRequest const &req) = 0;

        /**
         * Execute one or more multiple operations as one atomic operation.
         *
         * @param req
         * @return Either the EtcdTxnResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdTxnResponse, EtcdError>> txn(EtcdTxnRequest const &req) = 0;

        /**
         * Compact the Etcd history.
         *
         * @param req
         * @return Either the EtcdCompactionResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdCompactionResponse, EtcdError>> compact(EtcdCompactionRequest const &req) = 0;

        /**
         * Request a lease
         *
         * @param req
         * @return Either the LeaseGrantResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<LeaseGrantResponse, EtcdError>> leaseGrant(LeaseGrantRequest const &req) = 0;

        /**
         * Revoke a lease
         *
         * @param req
         * @return Either the LeaseRevokeResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<LeaseRevokeResponse, EtcdError>> leaseRevoke(LeaseRevokeRequest const &req) = 0;

        /**
         * Keep an existing lease alive
         *
         * @param req
         * @return Either the LeaseKeepAliveResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<LeaseKeepAliveResponse, EtcdError>> leaseKeepAlive(LeaseKeepAliveRequest const &req) = 0;

        /**
         * Get information on an existing lease
         *
         * @param req
         * @return Either the LeaseTimeToLiveResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<LeaseTimeToLiveResponse, EtcdError>> leaseTimeToLive(LeaseTimeToLiveRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the LeaseLeasesResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<LeaseLeasesResponse, EtcdError>> leaseLeases(LeaseLeasesRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthEnableResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthEnableResponse, EtcdError>> authEnable(AuthEnableRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthDisableResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthDisableResponse, EtcdError>> authDisable(AuthDisableRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthStatusResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthStatusResponse, EtcdError>> authStatus(AuthStatusRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthenticateResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthenticateResponse, EtcdError>> authenticate(AuthenticateRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthUserAddResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthUserAddResponse, EtcdError>> userAdd(AuthUserAddRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthUserGetResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthUserGetResponse, EtcdError>> userGet(AuthUserGetRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthUserListResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthUserListResponse, EtcdError>> userList(AuthUserListRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthUserDeleteResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthUserDeleteResponse, EtcdError>> userDelete(AuthUserDeleteRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthUserChangePasswordResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthUserChangePasswordResponse, EtcdError>> userChangePassword(AuthUserChangePasswordRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthUserGrantRoleResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthUserGrantRoleResponse, EtcdError>> userGrantRole(AuthUserGrantRoleRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthUserRevokeRoleResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthUserRevokeRoleResponse, EtcdError>> userRevokeRole(AuthUserRevokeRoleRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthRoleAddResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthRoleAddResponse, EtcdError>> roleAdd(AuthRoleAddRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the LeaseLeasesResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthRoleGetResponse, EtcdError>> roleGet(AuthRoleGetRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthRoleListResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthRoleListResponse, EtcdError>> roleList(AuthRoleListRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthRoleDeleteResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthRoleDeleteResponse, EtcdError>> roleDelete(AuthRoleDeleteRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthRoleGrantPermissionResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthRoleGrantPermissionResponse, EtcdError>> roleGrantPermission(AuthRoleGrantPermissionRequest const &req) = 0;

        /**
         * Get a list of existing leases
         *
         * @param req
         * @return Either the AuthRoleRevokePermissionResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<AuthRoleRevokePermissionResponse, EtcdError>> roleRevokePermission(AuthRoleRevokePermissionRequest const &req) = 0;

        /**
         * Get the version of the etcd server that we're connected to
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdVersionReply, EtcdError>> version() = 0;

        /**
         * Get the etcd server version that was detected during start.
         * May terminate the program if called before service is started.
         * @return
         */
        [[nodiscard]] virtual Version getDetectedVersion() const = 0;

        /**
         * Get the health status of the etcd server that we're connected to
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<bool, EtcdError>> health() = 0;

        /**
         * Sets the authentication to use for each request. Caution: might store password in memory.
         * @param user
         * @param password
         */
        virtual void setAuthentication(std::string_view user, std::string_view password) = 0;

        /**
         * Clears used authentication
         */
        virtual void clearAuthentication() = 0;

        /**
         * Gets the user used with the current authentication
         */
        [[nodiscard]] virtual tl::optional<std::string> getAuthenticationUser() const = 0;

    protected:
        ~IEtcd() = default;
    };
}

template <>
struct fmt::formatter<Ichor::Etcd::v3::EtcdError> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Etcd::v3::EtcdError& state, FormatContext& ctx) {
        switch(state) {
            case Ichor::Etcd::v3::EtcdError::HTTP_RESPONSE_ERROR:
                return fmt::format_to(ctx.out(), "HTTP_RESPONSE_ERROR");
            case Ichor::Etcd::v3::EtcdError::JSON_PARSE_ERROR:
                return fmt::format_to(ctx.out(), "JSON_PARSE_ERROR");
            case Ichor::Etcd::v3::EtcdError::VERSION_PARSE_ERROR:
                return fmt::format_to(ctx.out(), "VERSION_PARSE_ERROR");
            case Ichor::Etcd::v3::EtcdError::TIMEOUT:
                return fmt::format_to(ctx.out(), "TIMEOUT");
            case Ichor::Etcd::v3::EtcdError::CONNECTION_CLOSED_PREMATURELY_TRY_AGAIN:
                return fmt::format_to(ctx.out(), "CONNECTION_CLOSED_PREMATURELY_TRY_AGAIN");
            case Ichor::Etcd::v3::EtcdError::ROOT_USER_NOT_YET_CREATED:
                return fmt::format_to(ctx.out(), "ROOT_USER_NOT_YET_CREATED");
            case Ichor::Etcd::v3::EtcdError::REMOVING_ROOT_NOT_ALLOWED_WITH_AUTH_ENABLED:
                return fmt::format_to(ctx.out(), "REMOVING_ROOT_NOT_ALLOWED_WITH_AUTH_ENABLED");
            case Ichor::Etcd::v3::EtcdError::NO_AUTHENTICATION_SET:
                return fmt::format_to(ctx.out(), "NO_AUTHENTICATION_SET");
            case Ichor::Etcd::v3::EtcdError::UNAUTHORIZED:
                return fmt::format_to(ctx.out(), "UNAUTHORIZED");
            case Ichor::Etcd::v3::EtcdError::NOT_FOUND:
                return fmt::format_to(ctx.out(), "NOT_FOUND");
            case Ichor::Etcd::v3::EtcdError::DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT:
                return fmt::format_to(ctx.out(), "DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT");
            case Ichor::Etcd::v3::EtcdError::CANNOT_DELETE_ROOT_WHILE_AUTH_IS_ENABLED:
                return fmt::format_to(ctx.out(), "CANNOT_DELETE_ROOT_WHILE_AUTH_IS_ENABLED");
            case Ichor::Etcd::v3::EtcdError::QUITTING:
                return fmt::format_to(ctx.out(), "QUITTING");
            case Ichor::Etcd::v3::EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT:
                return fmt::format_to(ctx.out(), "ETCD_SERVER_DOES_NOT_SUPPORT");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};

template <>
struct fmt::formatter<Ichor::Etcd::v3::EtcdEventType> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Etcd::v3::EtcdEventType& state, FormatContext& ctx) {
        switch(state) {
            case Ichor::Etcd::v3::EtcdEventType::PUT:
                return fmt::format_to(ctx.out(), "PUT");
            case Ichor::Etcd::v3::EtcdEventType::DELETE_:
                return fmt::format_to(ctx.out(), "DELETE");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};

template <>
struct fmt::formatter<Ichor::Etcd::v3::EtcdSortOrder> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Etcd::v3::EtcdSortOrder& state, FormatContext& ctx) {
        switch(state) {
            case Ichor::Etcd::v3::EtcdSortOrder::NONE:
                return fmt::format_to(ctx.out(), "NONE");
            case Ichor::Etcd::v3::EtcdSortOrder::ASCEND:
                return fmt::format_to(ctx.out(), "ASCEND");
            case Ichor::Etcd::v3::EtcdSortOrder::DESCEND:
                return fmt::format_to(ctx.out(), "DESCEND");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};

template <>
struct fmt::formatter<Ichor::Etcd::v3::EtcdSortTarget> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Etcd::v3::EtcdSortTarget& state, FormatContext& ctx) {
        switch(state) {
            case Ichor::Etcd::v3::EtcdSortTarget::KEY:
                return fmt::format_to(ctx.out(), "KEY");
            case Ichor::Etcd::v3::EtcdSortTarget::VERSION:
                return fmt::format_to(ctx.out(), "VERSION");
            case Ichor::Etcd::v3::EtcdSortTarget::CREATE:
                return fmt::format_to(ctx.out(), "CREATE");
            case Ichor::Etcd::v3::EtcdSortTarget::MOD:
                return fmt::format_to(ctx.out(), "MOD");
            case Ichor::Etcd::v3::EtcdSortTarget::VALUE:
                return fmt::format_to(ctx.out(), "VALUE");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};

template <>
struct fmt::formatter<Ichor::Etcd::v3::EtcdCompareResult> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Etcd::v3::EtcdCompareResult& state, FormatContext& ctx) {
        switch(state) {
            case Ichor::Etcd::v3::EtcdCompareResult::EQUAL:
                return fmt::format_to(ctx.out(), "EQUAL");
            case Ichor::Etcd::v3::EtcdCompareResult::GREATER:
                return fmt::format_to(ctx.out(), "GREATER");
            case Ichor::Etcd::v3::EtcdCompareResult::LESS:
                return fmt::format_to(ctx.out(), "LESS");
            case Ichor::Etcd::v3::EtcdCompareResult::NOT_EQUAL:
                return fmt::format_to(ctx.out(), "NOT_EQUAL");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};

template <>
struct fmt::formatter<Ichor::Etcd::v3::EtcdCompareTarget> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Etcd::v3::EtcdCompareTarget& state, FormatContext& ctx) {
        switch(state) {
            case Ichor::Etcd::v3::EtcdCompareTarget::VERSION:
                return fmt::format_to(ctx.out(), "VERSION");
            case Ichor::Etcd::v3::EtcdCompareTarget::CREATE:
                return fmt::format_to(ctx.out(), "CREATE");
            case Ichor::Etcd::v3::EtcdCompareTarget::MOD:
                return fmt::format_to(ctx.out(), "MOD");
            case Ichor::Etcd::v3::EtcdCompareTarget::VALUE:
                return fmt::format_to(ctx.out(), "VALUE");
            case Ichor::Etcd::v3::EtcdCompareTarget::LEASE:
                return fmt::format_to(ctx.out(), "LEASE");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};
