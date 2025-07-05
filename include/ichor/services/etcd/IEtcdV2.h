#pragma once

#include <ichor/coroutines/Task.h>
#include <string_view>
#include <optional> // Glaze does not support tl::optional
#include <tl/optional.h>
#include <string>
#include <tl/expected.h>
#include <fmt/base.h>

namespace Ichor::Etcdv2::v1 {
    enum class EtcdError : uint_fast16_t {
        HTTP_RESPONSE_ERROR,
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
        HTTP_SEND_ERROR
    };

    enum class EtcdErrorCodes : uint_fast16_t {
        KEY_DOES_NOT_EXIST = 100,
        COMPARE_AND_SWAP_FAILED = 101,
        NOT_A_FILE = 102,
        NOT_A_DIRECTORY = 104,
        KEY_ALREADY_EXISTS = 105,
        ROOT_IS_READ_ONLY = 107,
        DIRECTORY_NOT_EMPTY = 108,
        PREVIOUS_VALUE_REQUIRED = 201,
        TTL_IS_NOT_A_NUMBER = 202,
        INDEX_IS_NOT_A_NUMBER = 203,
        INVALID_FIELD = 209,
        INVALID_FORM = 210,
        RAFT_INTERNAL_ERROR = 300,
        LEADER_ELECTRION_ERROR = 301,
        WATCHER_CLEARED_DUE_TO_RECOVERY = 400,
        EVENT_INDEX_OUTDATED_AND_CLEARED = 401,
    };

    struct EtcdReplyNode final {
        uint64_t createdIndex;
        uint64_t modifiedIndex;
        std::string key;
        std::optional<std::string> value;
        std::optional<uint64_t> ttl;
        std::optional<std::string> expiration;
        std::optional<bool> dir;
        std::optional<std::vector<EtcdReplyNode>> nodes;
    };

    struct EtcdReply final {
        std::optional<std::string> action;
        std::optional<EtcdReplyNode> node;
        std::optional<EtcdReplyNode> prevNode;
        std::optional<uint64_t> index;

        std::optional<std::string> cause;
        std::optional<EtcdErrorCodes> errorCode;
        std::optional<std::string> message;

        std::optional<std::string> x_etcd_cluster_id;
        std::optional<uint64_t> x_etcd_index;
        std::optional<uint64_t> x_raft_index;
        std::optional<uint64_t> x_raft_term;
    };

    struct EtcdLeaderInfoStats final {
        std::string leader;
        std::string uptime;
        std::string startTime;
    };

    struct EtcdSelfStats final {
        std::string name;
        std::string id;
        std::string state;
        std::string startTime;
        EtcdLeaderInfoStats leaderInfo;
        uint64_t recvAppendRequestCnt;
        uint64_t sendAppendRequestCnt;
    };

    struct EtcdStoreStats final {
        uint64_t compareAndSwapFail;
        uint64_t compareAndSwapSuccess;
        uint64_t compareAndDeleteSuccess;
        uint64_t compareAndDeleteFail;
        uint64_t createFail;
        uint64_t createSuccess;
        uint64_t deleteFail;
        uint64_t deleteSuccess;
        uint64_t expireCount;
        uint64_t getsFail;
        uint64_t getsSuccess;
        uint64_t setsFail;
        uint64_t setsSuccess;
        uint64_t updateFail;
        uint64_t updateSuccess;
        uint64_t watchers;
    };

    struct EtcdVersionReply final {
        std::string etcdserver;
        std::string etcdcluster;
    };

    struct EtcdKvReply final {
        std::vector<std::string> read;
        std::vector<std::string> write;
    };

    struct EtcdPermissionsReply final {
        EtcdKvReply kv;
    };

    struct EtcdRoleReply final {
        std::string role;
        EtcdPermissionsReply permissions;
        std::optional<EtcdPermissionsReply> grant;
        std::optional<EtcdPermissionsReply> revoke;
    };

    struct EtcdRolesReply final {
        std::vector<EtcdRoleReply> roles;
    };

    struct EtcdUserReply final {
        std::string user;
        std::optional<std::string> password;
        std::vector<EtcdRoleReply> roles;
        std::optional<std::vector<std::string>> grant;
        std::optional<std::vector<std::string>> revoke;
    };

    struct EtcdUpdateUserReply final {
        std::string user;
        std::optional<std::vector<std::string>> roles;
    };

    struct EtcdUsersReply final {
        std::optional<std::vector<EtcdUserReply>> users;
    };

    class IEtcd {
    public:
        /**
         * Create or update or compare-and-swap operation
         *
         * @param key key to update/create
         * @param value value to set to
         * @param previous_value optional: set to true to only update the key if the current value equals previous_value, leave nullopt to get the default etcd behaviour
         * @param previous_index optional: set to a specific to only update the key if the current indedx equals previous_index, leave nullopt or set to 0 get the default etcd behaviour
         * @param previous_exists optional: set to true to only update the key if the key already exists, set to false to only create a key and not update, leave nullopt to enable both
         * @param ttl_second optional: how many seconds should this key last
         * @param refresh set to true to only refresh the TTL or value
         * @param dir set to true to create/update a directory
         * @param in_order set to true to change to a POST request and create in-order keys, most commonly used for queues.
         * @return Either the EtcdReply or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, tl::optional<std::string_view> previous_value, tl::optional<uint64_t> previous_index, tl::optional<bool> previous_exists, tl::optional<uint64_t> ttl_seconds, bool refresh, bool dir, bool in_order) = 0;
        /**
         * Create or update operation
         *
         * @param key key to update/create
         * @param value value to set to
         * @param ttl_second optional: how many seconds should this key last
         * @param refresh set to true to only refresh the TTL or value
         * @return Either the EtcdReply or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, tl::optional<uint64_t> ttl_seconds, bool refresh) = 0;
        /**
         * Create or update operation
         *
         * @param key key to update/create
         * @param value value to set to
         * @param ttl_second optional: how many seconds should this key last
         * @return Either the EtcdReply or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, tl::optional<uint64_t> ttl_seconds) = 0;
        /**
         * Create or update operation
         *
         * @param key key to update/create
         * @param value value to set to
         * @return Either the EtcdReply or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value) = 0;

        /**
         * Get value for key operation
         *
         * @param key
         * @param recursive Set to true if key is a directory and you want all underlying keys/values
         * @param sorted Set to true if
         * @return Either the EtcdReply or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdReply, EtcdError>> get(std::string_view key, bool recursive, bool sorted, bool watch, tl::optional<uint64_t> watchIndex) = 0;

        /**
         * Get value for key operation
         *
         * @param key
         * @return Either the EtcdReply or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdReply, EtcdError>> get(std::string_view key) = 0;

        /**
         * Delete key operation
         *
         * @param key
         * @param recursive set to true if key is a directory and you want to recursively delete all keys. If dir has keys but delete is not recursive, delete will fail.
         * @param dir set to true if key is a directory
         * @return Either the EtcdReply or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdReply, EtcdError>> del(std::string_view key, bool recursive, bool dir) = 0;

        /**
         * Unimplemented
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdReply, EtcdError>> leaderStatistics() = 0;

        /**
         * Get statistics of the etcd server that we're connected to
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdSelfStats, EtcdError>> selfStatistics() = 0;

        /**
         * Get the store statistics of the etcd server that we're connected to
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdStoreStats, EtcdError>> storeStatistics() = 0;

        /**
         * Get the version of the etcd server that we're connected to
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdVersionReply, EtcdError>> version() = 0;

        /**
         * Get the health status of the etcd server that we're connected to
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<bool, EtcdError>> health() = 0;

        /**
         * Get auth status
         * @return true if auth has been enabled
         */
        [[nodiscard]] virtual Task<tl::expected<bool, EtcdError>> authStatus() = 0;

        /**
         * Enable auth. Requires root user to have been created and for the service to authenticate as the root user.
         * @return true if successful
         */
        [[nodiscard]] virtual Task<tl::expected<bool, EtcdError>> enableAuth() = 0;

        /**
         * Disable auth. Requires root user to have been created and for the service to authenticate as the root user.
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<bool, EtcdError>> disableAuth() = 0;

        /**
         * Get a list of all users details
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdUsersReply, EtcdError>> getUsers() = 0;

        /**
         * Get user details for given user
         * @param user
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdUserReply, EtcdError>> getUserDetails(std::string_view user) = 0;

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

        /**
         * Create user
         * @param user
         * @param pass
         * @return full user details
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdUpdateUserReply, EtcdError>> createUser(std::string_view user, std::string_view pass) = 0;

        /**
         * Grant roles to user
         * @param user
         * @param roles list of roles for this user
         * @return updated full user details
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdUpdateUserReply, EtcdError>> grantUserRoles(std::string_view user, std::vector<std::string> roles) = 0;

        /**
         * Revoke roles of user
         * @param user
         * @param roles list of roles for this user
         * @return updated full user details
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdUpdateUserReply, EtcdError>> revokeUserRoles(std::string_view user, std::vector<std::string> roles) = 0;

        /**
         * Revoke roles of user
         * @param user
         * @param pass
         * @return updated full user details
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdUpdateUserReply, EtcdError>> updateUserPassword(std::string_view user, std::string_view pass) = 0;

        /**
         * Delete user
         * @param user
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<void, EtcdError>> deleteUser(std::string_view user) = 0;

        /**
         * Get details for all roles
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdRolesReply, EtcdError>> getRoles() = 0;

        /**
         * Get details for specific role
         * @param user
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdRoleReply, EtcdError>> getRole(std::string_view role) = 0;

        /**
         * Create role
         * @param role name
         * @param read_permissions
         * @param write_permissions
         * @return full role details
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdRoleReply, EtcdError>> createRole(std::string_view role, std::vector<std::string> read_permissions, std::vector<std::string> write_permissions) = 0;

        /**
         * Grant role permissions
         * @param role name
         * @param read_permissions
         * @param write_permissions
         * @return updated full role details
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdRoleReply, EtcdError>> grantRolePermissions(std::string_view role, std::vector<std::string> read_permissions, std::vector<std::string> write_permissions) = 0;

        /**
         * Revoke role permissions
         * @param role name
         * @param read_permissions
         * @param write_permissions
         * @return updated full role details
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdRoleReply, EtcdError>> revokeRolePermissions(std::string_view role, std::vector<std::string> read_permissions, std::vector<std::string> write_permissions) = 0;

        /**
         * Delete role
         * @param role
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<void, EtcdError>> deleteRole(std::string_view role) = 0;





    protected:
        ~IEtcd() = default;
    };
}

template <>
struct fmt::formatter<Ichor::Etcdv2::v1::EtcdError> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Etcdv2::v1::EtcdError& state, FormatContext& ctx) const {
        switch(state) {
            case Ichor::Etcdv2::v1::EtcdError::HTTP_RESPONSE_ERROR:
                return fmt::format_to(ctx.out(), "HTTP_RESPONSE_ERROR");
            case Ichor::Etcdv2::v1::EtcdError::JSON_PARSE_ERROR:
                return fmt::format_to(ctx.out(), "JSON_PARSE_ERROR");
            case Ichor::Etcdv2::v1::EtcdError::TIMEOUT:
                return fmt::format_to(ctx.out(), "TIMEOUT");
            case Ichor::Etcdv2::v1::EtcdError::CONNECTION_CLOSED_PREMATURELY_TRY_AGAIN:
                return fmt::format_to(ctx.out(), "CONNECTION_CLOSED_PREMATURELY_TRY_AGAIN");
            case Ichor::Etcdv2::v1::EtcdError::ROOT_USER_NOT_YET_CREATED:
                return fmt::format_to(ctx.out(), "ROOT_USER_NOT_YET_CREATED");
            case Ichor::Etcdv2::v1::EtcdError::REMOVING_ROOT_NOT_ALLOWED_WITH_AUTH_ENABLED:
                return fmt::format_to(ctx.out(), "REMOVING_ROOT_NOT_ALLOWED_WITH_AUTH_ENABLED");
            case Ichor::Etcdv2::v1::EtcdError::NO_AUTHENTICATION_SET:
                return fmt::format_to(ctx.out(), "NO_AUTHENTICATION_SET");
            case Ichor::Etcdv2::v1::EtcdError::UNAUTHORIZED:
                return fmt::format_to(ctx.out(), "UNAUTHORIZED");
            case Ichor::Etcdv2::v1::EtcdError::NOT_FOUND:
                return fmt::format_to(ctx.out(), "NOT_FOUND");
            case Ichor::Etcdv2::v1::EtcdError::DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT:
                return fmt::format_to(ctx.out(), "DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT");
            case Ichor::Etcdv2::v1::EtcdError::CANNOT_DELETE_ROOT_WHILE_AUTH_IS_ENABLED:
                return fmt::format_to(ctx.out(), "CANNOT_DELETE_ROOT_WHILE_AUTH_IS_ENABLED");
            case Ichor::Etcdv2::v1::EtcdError::QUITTING:
                return fmt::format_to(ctx.out(), "QUITTING");
            case Ichor::Etcdv2::v1::EtcdError::HTTP_SEND_ERROR:
                return fmt::format_to(ctx.out(), "HTTP_SEND_ERROR");
        }
        return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
    }
};

template <>
struct fmt::formatter<Ichor::Etcdv2::v1::EtcdErrorCodes> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Etcdv2::v1::EtcdErrorCodes& state, FormatContext& ctx) const {
        switch(state)
        {
            case Ichor::Etcdv2::v1::EtcdErrorCodes::KEY_DOES_NOT_EXIST:
                return fmt::format_to(ctx.out(), "KEY_DOES_NOT_EXIST");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::COMPARE_AND_SWAP_FAILED:
                return fmt::format_to(ctx.out(), "COMPARE_AND_SWAP_FAILED");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::NOT_A_FILE:
                return fmt::format_to(ctx.out(), "NOT_A_FILE");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::NOT_A_DIRECTORY:
                return fmt::format_to(ctx.out(), "NOT_A_DIRECTORY");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::KEY_ALREADY_EXISTS:
                return fmt::format_to(ctx.out(), "KEY_ALREADY_EXISTS");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::ROOT_IS_READ_ONLY:
                return fmt::format_to(ctx.out(), "ROOT_IS_READ_ONLY");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::DIRECTORY_NOT_EMPTY:
                return fmt::format_to(ctx.out(), "DIRECTORY_NOT_EMPTY");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::PREVIOUS_VALUE_REQUIRED:
                return fmt::format_to(ctx.out(), "PREVIOUS_VALUE_REQUIRED");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::TTL_IS_NOT_A_NUMBER:
                return fmt::format_to(ctx.out(), "TTL_IS_NOT_A_NUMBER");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::INDEX_IS_NOT_A_NUMBER:
                return fmt::format_to(ctx.out(), "INDEX_IS_NOT_A_NUMBER");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::INVALID_FIELD:
                return fmt::format_to(ctx.out(), "INVALID_FIELD");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::INVALID_FORM:
                return fmt::format_to(ctx.out(), "INVALID_FORM");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::RAFT_INTERNAL_ERROR:
                return fmt::format_to(ctx.out(), "RAFT_INTERNAL_ERROR");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::LEADER_ELECTRION_ERROR:
                return fmt::format_to(ctx.out(), "LEADER_ELECTRION_ERROR");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::WATCHER_CLEARED_DUE_TO_RECOVERY:
                return fmt::format_to(ctx.out(), "WATCHER_CLEARED_DUE_TO_RECOVERY");
            case Ichor::Etcdv2::v1::EtcdErrorCodes::EVENT_INDEX_OUTDATED_AND_CLEARED:
                return fmt::format_to(ctx.out(), "EVENT_INDEX_OUTDATED_AND_CLEARED");
        }
        return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
    }
};
