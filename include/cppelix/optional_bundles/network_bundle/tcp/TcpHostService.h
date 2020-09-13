#pragma once

#include <cppelix/optional_bundles/network_bundle/IHostService.h>
#include <cppelix/optional_bundles/logging_bundle/Logger.h>
#include <cppelix/optional_bundles/network_bundle/tcp/TcpConnectionService.h>
#include <thread>

namespace Cppelix {
    class TcpHostService final : public IHostService, public Service {
    public:
        TcpHostService(DependencyRegister &reg, CppelixProperties props);
        ~TcpHostService() final = default;

        bool start() final;
        bool stop() final;

        void addDependencyInstance(ILogger *logger);
        void removeDependencyInstance(ILogger *logger);

        void set_priority(uint64_t priority) final;
        uint64_t get_priority() final;

    private:
        int _socket;
        int _bindFd;
        std::atomic<uint64_t> _priority;
        std::atomic<bool> _quit;
        std::thread _listenThread;
        ILogger *_logger{nullptr};
        std::vector<TcpConnectionService*> _connections;
    };
}