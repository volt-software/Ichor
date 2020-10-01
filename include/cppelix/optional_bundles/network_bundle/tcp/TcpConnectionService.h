#pragma once

#include <cppelix/optional_bundles/network_bundle/IConnectionService.h>
#include <cppelix/optional_bundles/logging_bundle/Logger.h>
#include <thread>

namespace Cppelix {
    class TcpConnectionService final : public IConnectionService, public Service {
    public:
        TcpConnectionService(DependencyRegister &reg, CppelixProperties props);
        ~TcpConnectionService() final = default;

        bool start() final;
        bool stop() final;

        void addDependencyInstance(ILogger *logger);
        void removeDependencyInstance(ILogger *logger);

        bool send(std::vector<uint8_t>&& msg) final;
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