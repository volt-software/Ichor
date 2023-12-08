#pragma once

#include <ichor/services/etcd/IEtcd.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/IClientFactory.h>
#include <stack>

namespace Ichor {

    struct ConnRequest final {
        IHttpConnectionService *conn{};
        AsyncManualResetEvent event{};
    };

    /**
     * Service for the redis protocol using the v2 REST API. Requires an IHttpConnectionService factory and a logger. See https://etcd.io/docs/v2.3/api/ for a detailed look at the etcd v2 API.
     *
     * Properties:
     * - "Address" - What address to connect to (required)
     * - "Port" - What port to connect to (required)
     */
    class EtcdV2Service final : public IEtcd, public AdvancedService<EtcdV2Service> {
    public:
        EtcdV2Service(DependencyRegister &reg, Properties props);
        ~EtcdV2Service() final = default;

        Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, std::optional<std::string_view> previous_value, std::optional<uint64_t> previous_index, std::optional<bool> previous_exists, std::optional<uint64_t> ttl_second, bool refresh, bool dir, bool in_order) final;
        Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, std::optional<uint64_t> ttl_second, bool refresh) final;
        Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, std::optional<uint64_t> ttl_second) final;
        Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value) final;

        Task<tl::expected<EtcdReply, EtcdError>> get(std::string_view key, bool recursive, bool sorted, bool watch, std::optional<uint64_t> watchIndex) final;
        Task<tl::expected<EtcdReply, EtcdError>> get(std::string_view key) final;
        Task<tl::expected<EtcdReply, EtcdError>> del(std::string_view key, bool recursive, bool dir) final;
        /// Not implemented, the leader statistics have variable key names in the response which is harder to decode with glaze.
        /// \return Always returns EtcdError::JSON_PARSE_ERROR
        Task<tl::expected<EtcdReply, EtcdError>> leaderStatistics() final;
        Task<tl::expected<EtcdSelfStats, EtcdError>> selfStatistics() final;
        Task<tl::expected<EtcdStoreStats, EtcdError>> storeStatistics() final;
        Task<tl::expected<EtcdVersionReply, EtcdError>> version() final;
        Task<tl::expected<bool, EtcdError>> health() final;

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
    };
}
