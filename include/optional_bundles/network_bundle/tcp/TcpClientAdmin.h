#pragma once

#include "optional_bundles/network_bundle/IClientAdmin.h"
#include <optional_bundles/logging_bundle/Logger.h>
#include <optional_bundles/network_bundle/tcp/TcpConnectionService.h>
#include <thread>
#include <framework/DependencyManager.h>

namespace Cppelix {
    class TcpClientAdmin final : public IClientAdmin, public Service {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        TcpClientAdmin() = default;
        ~TcpClientAdmin() override = default;

        bool start() final;
        bool stop() final;

        void addDependencyInstance(ILogger *logger);
        void removeDependencyInstance(ILogger *logger);

        void handleDependencyRequest(IConnectionService*, DependencyRequestEvent const * const evt);
        void handleDependencyUndoRequest(IConnectionService*, DependencyUndoRequestEvent const * const evt);

    private:
        ILogger *_logger{nullptr};
        std::unordered_map<uint64_t, IConnectionService*> _connections{};
        std::unique_ptr<DependencyTrackerRegistration> _trackerRegistration{nullptr};
    };
}