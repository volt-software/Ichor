#pragma once

#include <ichor/services/etcd/IEtcdV2.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/IClientFactory.h>
#include <stack>

namespace Ichor::Etcdv2::v1 {

    struct ConnRequest final {
        Ichor::v1::IHttpConnectionService *conn{};
        AsyncManualResetEvent event{};
    };

    /**
     * Service for the etcd protocol using the v2 REST API. Requires an IHttpConnectionService factory and a logger. See https://etcd.io/docs/v2.3/api/ for a detailed look at the etcd v2 API.
     *
     * Properties:
     * - "Address" std::string - What address to connect to (required)
     * - "Port" uint16_t - What port to connect to (required)
     */
    class EtcdService final : public IEtcd, public AdvancedService<EtcdService> {
    public:
        EtcdService(DependencyRegister &reg, Properties props);
        ~EtcdService() final = default;

        [[nodiscard]] Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, tl::optional<std::string_view> previous_value, tl::optional<uint64_t> previous_index, tl::optional<bool> previous_exists, tl::optional<uint64_t> ttl_second, bool refresh, bool dir, bool in_order) final;
        [[nodiscard]] Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, tl::optional<uint64_t> ttl_second, bool refresh) final;
        [[nodiscard]] Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, tl::optional<uint64_t> ttl_second) final;
        [[nodiscard]] Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value) final;

        [[nodiscard]] Task<tl::expected<EtcdReply, EtcdError>> get(std::string_view key, bool recursive, bool sorted, bool watch, tl::optional<uint64_t> watchIndex) final;
        [[nodiscard]] Task<tl::expected<EtcdReply, EtcdError>> get(std::string_view key) final;
        [[nodiscard]] Task<tl::expected<EtcdReply, EtcdError>> del(std::string_view key, bool recursive, bool dir) final;
        /// Not implemented, the leader statistics have variable key names in the response which is harder to decode with glaze.
        /// \return Always returns EtcdError::JSON_PARSE_ERROR
        [[nodiscard]] Task<tl::expected<EtcdReply, EtcdError>> leaderStatistics() final;
        [[nodiscard]] Task<tl::expected<EtcdSelfStats, EtcdError>> selfStatistics() final;
        [[nodiscard]] Task<tl::expected<EtcdStoreStats, EtcdError>> storeStatistics() final;
        [[nodiscard]] Task<tl::expected<EtcdVersionReply, EtcdError>> version() final;
        [[nodiscard]] Task<tl::expected<bool, EtcdError>> health() final;
        [[nodiscard]] Task<tl::expected<bool, EtcdError>> authStatus() final;
        [[nodiscard]] Task<tl::expected<bool, EtcdError>> enableAuth() final;
        [[nodiscard]] Task<tl::expected<bool, EtcdError>> disableAuth() final;
        [[nodiscard]] Task<tl::expected<EtcdUsersReply, EtcdError>> getUsers() final;
        [[nodiscard]] Task<tl::expected<EtcdUserReply, EtcdError>> getUserDetails(std::string_view user) final;
        void setAuthentication(std::string_view user, std::string_view pass) final;
        void clearAuthentication() final;
        [[nodiscard]] tl::optional<std::string> getAuthenticationUser() const final;
        [[nodiscard]] Task<tl::expected<EtcdUpdateUserReply, EtcdError>> createUser(std::string_view user, std::string_view pass) final;
        [[nodiscard]] Task<tl::expected<EtcdUpdateUserReply, EtcdError>> grantUserRoles(std::string_view user, std::vector<std::string> roles) final;
        [[nodiscard]] Task<tl::expected<EtcdUpdateUserReply, EtcdError>> revokeUserRoles(std::string_view user, std::vector<std::string> roles) final;
        [[nodiscard]] Task<tl::expected<EtcdUpdateUserReply, EtcdError>> updateUserPassword(std::string_view user, std::string_view pass) final;
        [[nodiscard]] Task<tl::expected<void, EtcdError>> deleteUser(std::string_view user) final;
        [[nodiscard]] Task<tl::expected<EtcdRolesReply, EtcdError>> getRoles() final;
        [[nodiscard]] Task<tl::expected<EtcdRoleReply, EtcdError>> getRole(std::string_view role) final;
        [[nodiscard]] Task<tl::expected<EtcdRoleReply, EtcdError>> createRole(std::string_view role, std::vector<std::string> read_permissions, std::vector<std::string> write_permissions) final;
        [[nodiscard]] Task<tl::expected<EtcdRoleReply, EtcdError>> grantRolePermissions(std::string_view role, std::vector<std::string> read_permissions, std::vector<std::string> write_permissions) final;
        [[nodiscard]] Task<tl::expected<EtcdRoleReply, EtcdError>> revokeRolePermissions(std::string_view role, std::vector<std::string> read_permissions, std::vector<std::string> write_permissions) final;
        [[nodiscard]] Task<tl::expected<void, EtcdError>> deleteRole(std::string_view role) final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(Ichor::v1::ILogger &logger, IService &isvc);
        void removeDependencyInstance(Ichor::v1::ILogger &logger, IService &isvc);

        void addDependencyInstance(Ichor::v1::IHttpConnectionService &conn, IService &isvc);
        void removeDependencyInstance(Ichor::v1::IHttpConnectionService &conn, IService &isvc);

        void addDependencyInstance(Ichor::v1::IClientFactory<Ichor::v1::IHttpConnectionService> &conn, IService &isvc);
        void removeDependencyInstance(Ichor::v1::IClientFactory<Ichor::v1::IHttpConnectionService> &conn, IService &isvc);

        friend DependencyRegister;

        Ichor::v1::ILogger *_logger{};
        Ichor::v1::IHttpConnectionService* _mainConn{};
        Ichor::v1::IClientFactory<Ichor::v1::IHttpConnectionService> *_clientFactory{};
        std::stack<ConnRequest> _connRequests{};
        tl::optional<std::string> _auth;
    };
}
