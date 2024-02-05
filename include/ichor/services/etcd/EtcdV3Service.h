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
     * - "Address" - What address to connect to (required)
     * - "Port" - What port to connect to (required)
     */
    class EtcdService final : public IEtcd, public AdvancedService<EtcdService> {
    public:
        EtcdService(DependencyRegister &reg, Properties props);
        ~EtcdService() final = default;

        [[nodiscard]] Task<tl::expected<EtcdPutResponse, EtcdError>> put(EtcdPutRequest const &req) final;
        [[nodiscard]] Task<tl::expected<EtcdRangeResponse, EtcdError>> range(EtcdRangeRequest const &req) final;
        [[nodiscard]] Task<tl::expected<EtcdDeleteRangeResponse, EtcdError>> deleteRange(EtcdDeleteRangeRequest const &req) final;
        [[nodiscard]] Task<tl::expected<EtcdVersionReply, EtcdError>> version() final;
        [[nodiscard]] Version getDetectedVersion() const final;
        [[nodiscard]] Task<tl::expected<bool, EtcdError>> health() final;
        void setAuthentication(std::string_view user, std::string_view pass) final;
        void clearAuthentication() final;
        [[nodiscard]] tl::optional<std::string> getAuthenticationUser() const final;

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
        Version _detectedVersion{};
        std::string_view _versionSpecificUrl{"/v3"};
    };
}
