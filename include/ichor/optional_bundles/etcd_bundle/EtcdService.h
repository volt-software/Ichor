#pragma once

#include <ichor/optional_bundles/etcd_bundle/IEtcdService.h>
#include <ichor/optional_bundles/etcd_bundle/rpc.grpc.pb.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <thread>

namespace Ichor {
    class EtcdService final : public IEtcdService, public Service<EtcdService> {
    public:
        EtcdService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng);
        ~EtcdService() final = default;

        bool start() final;
        bool stop() final;

        void addDependencyInstance(ILogger *logger);
        void removeDependencyInstance(ILogger *logger);

        bool put(std::string&& key, std::string&& value) final;
        std::optional<std::string> get(std::string&& key) final;

    private:
        ILogger *_logger{nullptr};
        std::shared_ptr<grpc::Channel> _channel{nullptr};
        std::unique_ptr<etcdserverpb::KV::Stub> _stub;
    };
}