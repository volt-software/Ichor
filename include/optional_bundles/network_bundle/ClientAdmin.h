#pragma once

#include "optional_bundles/network_bundle/IClientAdmin.h"
#include <optional_bundles/logging_bundle/Logger.h>
#include <thread>
#include <framework/DependencyManager.h>

namespace Cppelix {
    template <typename NetworkType>
    class ClientAdmin final : public IClientAdmin, public Service {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        ClientAdmin() = default;
        ~ClientAdmin() override = default;

        bool start() final {
            _trackerRegistration = getManager()->template registerDependencyTracker<IConnectionService>(getServiceId(), this);

            return false;
        }

        bool stop() final {
            _trackerRegistration.reset(nullptr);
            return false;
        }

        void handleDependencyRequest(IConnectionService*, DependencyRequestEvent const * const evt) {
            if(!evt->properties->contains("Address")) {
                throw std::runtime_error("Missing address");
            }

            if(!evt->properties->contains("Port")) {
                throw std::runtime_error("Missing port");
            }

            auto newProps = *evt->properties;
            newProps.emplace("Filter", Filter{ServiceIdFilterEntry{evt->originatingService}});

            if(!_connections.contains(evt->originatingService)) {
                _connections.emplace(evt->originatingService, getManager()->template createServiceManager<IConnectionService, NetworkType>(RequiredList<ILogger>, OptionalList<>, newProps));
            }
        }

        void handleDependencyUndoRequest(IConnectionService*, DependencyUndoRequestEvent const * const evt) {
            auto connection = _connections.find(evt->originatingService);

            if(connection != end(_connections)) {
                getManager()->template pushEvent<RemoveServiceEvent>(evt->originatingService, connection->second->getServiceId());
                _connections.erase(connection);
            }
        }

    private:
        ILogger *_logger{nullptr};
        std::unordered_map<uint64_t, IConnectionService*> _connections{};
        std::unique_ptr<DependencyTrackerRegistration> _trackerRegistration{nullptr};
    };
}