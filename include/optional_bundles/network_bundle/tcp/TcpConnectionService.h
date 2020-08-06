#pragma once

#include "optional_bundles/network_bundle/IConnectionService.h"
#include <optional_bundles/logging_bundle/Logger.h>
#include <thread>

namespace Cppelix {
    class TcpConnectionService final : public IConnectionService, public Service {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        TcpConnectionService();
        ~TcpConnectionService() override = default;

        bool start() final;
        bool stop() final;

        void addDependencyInstance(ILogger *logger);
        void removeDependencyInstance(ILogger *logger);

        void send(std::vector<uint8_t>&& msg) final;
        void set_priority(uint64_t priority) final;
        uint64_t get_priority() final;

    private:
        int _socket;
        int _attempts;
        std::atomic<uint64_t> _priority;
        std::atomic<bool> _quit;
        std::thread _listenThread;
        ILogger *_logger{nullptr};
    };
}