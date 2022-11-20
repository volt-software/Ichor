#pragma once

#include <ichor/services/etcd/IEtcdService.h>
#include <ichor/services/etcd/rpc.grpc.pb.h>
#include <ichor/services/logging/Logger.h>
#include <thread>

namespace Ichor {
    class EtcdService final : public IEtcdService, public Service<EtcdService> {
    public:
        EtcdService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~EtcdService() final = default;

        bool put(std::string&& key, std::string&& value) final;
        std::optional<std::string> get(std::string&& key) final;

    private:
        AsyncGenerator<void> start() final;
        AsyncGenerator<void> stop() final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);

        friend DependencyRegister;

        ILogger *_logger{nullptr};
        std::shared_ptr<grpc::Channel> _channel{nullptr};
        std::unique_ptr<etcdserverpb::KV::Stub> _stub; // cannot use ichor unique_ptr with generated code :(
    };
}