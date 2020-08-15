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
            _unrecoverableErrorRegistration = getManager()->template registerEventHandler<UnrecoverableErrorEvent>(getServiceId(), this);

            return false;
        }

        bool stop() final {
            _trackerRegistration = nullptr;
            _unrecoverableErrorRegistration = nullptr;
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

        Generator<bool> handleEvent(UnrecoverableErrorEvent const * const evt) {
            for(auto &[key, service] : _connections) {
                if(service->getServiceId() != evt->originatingService) {
                    continue;
                }

                auto address = service->getProperties()->find("Address");
                auto port = service->getProperties()->find("Port");
                auto full_address = std::any_cast<std::string>(address->second);
                if(port != end(*service->getProperties())) {
                    full_address += ":" + std::to_string(std::any_cast<uint16_t>(port->second));
                }
                std::string_view implNameRequestor = getManager()->getImplementationNameFor(evt->originatingService).value();
                LOG_ERROR(_logger, "Couldn't start connection of type {} on address {} for service of type {} with id {} because \"{}\"", typeNameHash<NetworkType>(), full_address, implNameRequestor, service->getServiceId(), evt->error);

                break;
            }

            // maybe others want to log this as well
            co_return (bool)AllowOthersHandling;
        }

    private:
        ILogger *_logger{nullptr};
        std::unordered_map<uint64_t, IConnectionService*> _connections{};
        std::unique_ptr<DependencyTrackerRegistration> _trackerRegistration{nullptr};
        std::unique_ptr<EventHandlerRegistration> _unrecoverableErrorRegistration{nullptr};
    };
}