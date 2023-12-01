#pragma once

#include <ichor/services/etcd/IEtcd.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/network/http/IHttpConnectionService.h>

namespace Ichor {

    /**
     * Service for the redis protocol using the v2 REST API. Requires an IHttpConnectionService factory and a logger. See https://etcd.io/docs/v2.3/api/ for a detailed look at the etcd v2 API.
     *
     * Properties:
     * - "Address" - What address to connect to (required)
     * - "Port" - What port to connect to (required)
     */
    class EtcdService final : public IEtcd, public AdvancedService<EtcdService> {
    public:
        EtcdService(DependencyRegister &reg, Properties props);
        ~EtcdService() final = default;

        Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, std::optional<std::string_view> previous_value, std::optional<uint64_t> previous_index, std::optional<bool> previous_exists, std::optional<uint64_t> ttl_second, bool refresh, bool dir, bool in_order) final;
        Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, std::optional<uint64_t> ttl_second, bool refresh) final;
        Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, std::optional<uint64_t> ttl_second) final;
        Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value) final;

        Task<tl::expected<EtcdReply, EtcdError>> get(std::string_view key, bool recursive, bool sorted) final;
        Task<tl::expected<EtcdReply, EtcdError>> get(std::string_view key) final;
        Task<tl::expected<EtcdReply, EtcdError>> del(std::string_view key, bool recursive, bool dir) final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService &isvc);
        void removeDependencyInstance(ILogger &logger, IService &isvc);

        void addDependencyInstance(IHttpConnectionService &conn, IService &isvc);
        void removeDependencyInstance(IHttpConnectionService &conn, IService &isvc);

        friend DependencyRegister;

        ILogger *_logger{};
        IHttpConnectionService *_conn{};
    };
}
