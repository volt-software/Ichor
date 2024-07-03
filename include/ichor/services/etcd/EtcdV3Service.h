#pragma once

#include <ichor/services/etcd/IEtcdV3.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/IClientFactory.h>
#include <stack>

namespace Ichor::Etcd::v3 {

    struct ConnRequest final {
        IHttpConnectionService *conn{};
        AsyncManualResetEvent event{};
    };

    /**
     * Service for the etcd protocol using the v3 REST API. Requires an IHttpConnectionService factory and a logger. See https://etcd.io/docs/v2.3/api/ for a detailed look at the etcd v2 API.
     *
     * Properties:
     * - "Address" std::string - What address to connect to (required)
     * - "Port" uint16_t - What port to connect to (required)
     */
    class EtcdService final : public IEtcd, public AdvancedService<EtcdService> {
    public:
        EtcdService(DependencyRegister &reg, Properties props);
        ~EtcdService() final = default;

        [[nodiscard]] Task<tl::expected<EtcdPutResponse, EtcdError>> put(EtcdPutRequest const &req) final;
        [[nodiscard]] Task<tl::expected<EtcdRangeResponse, EtcdError>> range(EtcdRangeRequest const &req) final;
        [[nodiscard]] Task<tl::expected<EtcdDeleteRangeResponse, EtcdError>> deleteRange(EtcdDeleteRangeRequest const &req) final;
        [[nodiscard]] Task<tl::expected<EtcdTxnResponse, EtcdError>> txn(EtcdTxnRequest const &req) final;
        [[nodiscard]] Task<tl::expected<EtcdCompactionResponse, EtcdError>> compact(EtcdCompactionRequest const &req) final;
        [[nodiscard]] Task<tl::expected<LeaseGrantResponse, EtcdError>> leaseGrant(LeaseGrantRequest const &req) final;
        [[nodiscard]] Task<tl::expected<LeaseRevokeResponse, EtcdError>> leaseRevoke(LeaseRevokeRequest const &req) final;
        [[nodiscard]] Task<tl::expected<LeaseKeepAliveResponse, EtcdError>> leaseKeepAlive(LeaseKeepAliveRequest const &req) final;
        [[nodiscard]] Task<tl::expected<LeaseTimeToLiveResponse, EtcdError>> leaseTimeToLive(LeaseTimeToLiveRequest const &req) final;
        [[nodiscard]] Task<tl::expected<LeaseLeasesResponse, EtcdError>> leaseLeases(LeaseLeasesRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthEnableResponse, EtcdError>> authEnable(AuthEnableRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthDisableResponse, EtcdError>> authDisable(AuthDisableRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthStatusResponse, EtcdError>> authStatus(AuthStatusRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthenticateResponse, EtcdError>> authenticate(AuthenticateRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthUserAddResponse, EtcdError>> userAdd(AuthUserAddRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthUserGetResponse, EtcdError>> userGet(AuthUserGetRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthUserListResponse, EtcdError>> userList(AuthUserListRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthUserDeleteResponse, EtcdError>> userDelete(AuthUserDeleteRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthUserChangePasswordResponse, EtcdError>> userChangePassword(AuthUserChangePasswordRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthUserGrantRoleResponse, EtcdError>> userGrantRole(AuthUserGrantRoleRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthUserRevokeRoleResponse, EtcdError>> userRevokeRole(AuthUserRevokeRoleRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthRoleAddResponse, EtcdError>> roleAdd(AuthRoleAddRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthRoleGetResponse, EtcdError>> roleGet(AuthRoleGetRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthRoleListResponse, EtcdError>> roleList(AuthRoleListRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthRoleDeleteResponse, EtcdError>> roleDelete(AuthRoleDeleteRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthRoleGrantPermissionResponse, EtcdError>> roleGrantPermission(AuthRoleGrantPermissionRequest const &req) final;
        [[nodiscard]] Task<tl::expected<AuthRoleRevokePermissionResponse, EtcdError>> roleRevokePermission(AuthRoleRevokePermissionRequest const &req) final;
        [[nodiscard]] Task<tl::expected<EtcdVersionReply, EtcdError>> version() final;
        [[nodiscard]] Version getDetectedVersion() const final;
        [[nodiscard]] Task<tl::expected<bool, EtcdError>> health() final;
        [[nodiscard]] tl::optional<std::string> const &getAuthenticationUser() const final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService &isvc);
        void removeDependencyInstance(ILogger &logger, IService &isvc);

        void addDependencyInstance(IHttpConnectionService &conn, IService &isvc);
        void removeDependencyInstance(IHttpConnectionService &conn, IService &isvc);

        void addDependencyInstance(IClientFactory &conn, IService &isvc);
        void removeDependencyInstance(IClientFactory &conn, IService &isvc);

        friend DependencyRegister;

        ILogger *_logger{};
        IHttpConnectionService* _mainConn{};
        IClientFactory *_clientFactory{};
        std::stack<ConnRequest> _connRequests{};
        tl::optional<std::string> _auth;
        tl::optional<std::string> _authUser;
        Version _detectedVersion{};
        std::string_view _versionSpecificUrl{"/v3"};
    };
}
