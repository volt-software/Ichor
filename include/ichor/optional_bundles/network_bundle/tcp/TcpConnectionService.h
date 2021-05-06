#pragma once

#include <ichor/optional_bundles/network_bundle/IConnectionService.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <thread>

namespace Ichor {
    class TcpConnectionService final : public IConnectionService, public Service<TcpConnectionService> {
    public:
        TcpConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~TcpConnectionService() final = default;

        bool start() final;
        bool stop() final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);

        bool send(std::pmr::vector<uint8_t>&& msg) final;
        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        int _socket;
        int _attempts;
        std::atomic<uint64_t> _priority;
        std::atomic<bool> _quit;
        std::thread _listenThread;
        ILogger *_logger{nullptr};
    };
}