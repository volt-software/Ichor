#pragma once

#include <ichor/optional_bundles/network_bundle/IClientAdmin.h>
#include <ichor/optional_bundles/network_bundle/IConnectionService.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <thread>
#include <ichor/DependencyManager.h>

namespace Ichor {
    template <typename NetworkType, typename NetworkInterfaceType = IConnectionService>
    class ClientAdmin final : public IClientAdmin, public Service<ClientAdmin<NetworkType, NetworkInterfaceType>> {
    public:
        ClientAdmin(Properties properties, DependencyManager *mng) : Service<ClientAdmin<NetworkType, NetworkInterfaceType>>(std::move(properties), mng), _connections{} {  }
        ~ClientAdmin() override = default;

        StartBehaviour start() final {
            _trackerRegistration = Service<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager()->template registerDependencyTracker<NetworkInterfaceType>(this);
            _unrecoverableErrorRegistration = Service<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager()->template registerEventHandler<UnrecoverableErrorEvent>(this);

            return StartBehaviour::SUCCEEDED;
        }

        StartBehaviour stop() final {
            _trackerRegistration.reset();
            _unrecoverableErrorRegistration.reset();
            return StartBehaviour::SUCCEEDED;
        }

        void handleDependencyRequest(NetworkInterfaceType*, DependencyRequestEvent const * const evt) {
            if(!evt->properties.has_value()) {
                throw std::runtime_error("Missing properties");
            }

            if(!evt->properties.value()->contains("Address")) {
                throw std::runtime_error("Missing address");
            }

            if(!evt->properties.value()->contains("Port")) {
                throw std::runtime_error("Missing port");
            }

            if(!_connections.contains(evt->originatingService)) {
                auto newProps = *evt->properties.value();
                newProps.emplace("Filter", Ichor::make_any<Filter>(ServiceIdFilterEntry{evt->originatingService}));

                _connections.emplace(evt->originatingService, Service<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager()->template createServiceManager<NetworkType, NetworkInterfaceType>(std::move(newProps)));
            }
        }

        void handleDependencyUndoRequest(NetworkInterfaceType*, DependencyUndoRequestEvent const * const evt) {
            auto connection = _connections.find(evt->originatingService);

            if(connection != end(_connections)) {
                Service<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager()->template pushEvent<RemoveServiceEvent>(evt->originatingService, connection->second->getServiceId());
                _connections.erase(connection);
            }
        }

        AsyncGenerator<bool> handleEvent(UnrecoverableErrorEvent const * const evt) {
            for(auto &[key, service] : _connections) {
                if(service->getServiceId() != evt->originatingService) {
                    continue;
                }

                auto const address = service->getProperties().find("Address");
                auto const port = service->getProperties().find("Port");
                auto full_address = Ichor::any_cast<std::string>(address->second);
                if(port != cend(service->getProperties())) {
                    full_address += ":" + std::to_string(Ichor::any_cast<uint16_t>(port->second));
                }
                std::string_view implNameRequestor = Service<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager()->getImplementationNameFor(evt->originatingService).value();
                ICHOR_LOG_ERROR(_logger, "Couldn't start connection of type {} on address {} for service of type {} with id {} because \"{}\"", typeNameHash<NetworkType>(), full_address, implNameRequestor, service->getServiceId(), evt->error);

                break;
            }

            // maybe others want to log this as well
            co_return (bool)AllowOthersHandling;
        }

    private:
        ILogger *_logger{nullptr};
        std::unordered_map<uint64_t, IService*> _connections;
        DependencyTrackerRegistration _trackerRegistration{};
        EventHandlerRegistration _unrecoverableErrorRegistration{};
    };
}