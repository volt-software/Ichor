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
        ClientAdmin(Properties properties, DependencyManager *mng) : Service<ClientAdmin<NetworkType, NetworkInterfaceType>>(std::move(properties), mng), _connections{this->getMemoryResource()} {  }
        ~ClientAdmin() override = default;

        bool start() final {
            _trackerRegistration = Service<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager()->template registerDependencyTracker<NetworkInterfaceType>(this);
            _unrecoverableErrorRegistration = Service<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager()->template registerEventHandler<UnrecoverableErrorEvent>(this);

            return false;
        }

        bool stop() final {
            _trackerRegistration = nullptr;
            _unrecoverableErrorRegistration = nullptr;
            return false;
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
                newProps.emplace("Filter", Ichor::make_any<Filter>(this->getMemoryResource(), Filter{this->getMemoryResource(), ServiceIdFilterEntry{evt->originatingService}}));

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

        Generator<bool> handleEvent(UnrecoverableErrorEvent const * const evt) {
            for(auto &[key, service] : _connections) {
                if(service->getServiceId() != evt->originatingService) {
                    continue;
                }

                auto address = service->getProperties()->find("Address");
                auto port = service->getProperties()->find("Port");
                auto full_address = Ichor::any_cast<std::string>(address->second);
                if(port != end(*service->getProperties())) {
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
        std::pmr::unordered_map<uint64_t, IService*> _connections;
        std::unique_ptr<DependencyTrackerRegistration, Deleter> _trackerRegistration{nullptr};
        std::unique_ptr<EventHandlerRegistration, Deleter> _unrecoverableErrorRegistration{nullptr};
    };
}