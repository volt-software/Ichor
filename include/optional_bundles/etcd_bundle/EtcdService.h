#pragma once

#include "optional_bundles/etcd_bundle/IEtcdService.h"
#include "optional_bundles/etcd_bundle/rpc.grpc.pb.h"
#include <optional_bundles/logging_bundle/Logger.h>
#include <thread>

namespace Cppelix {
    class EtcdService final : public IEtcdService, public Service {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        EtcdService(DependencyRegister &reg, CppelixProperties props);
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