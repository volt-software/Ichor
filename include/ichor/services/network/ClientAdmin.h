#pragma once

#include <ichor/services/network/IClientAdmin.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/logging/Logger.h>
#include <thread>
#include <ichor/DependencyManager.h>

namespace Ichor {
    template <typename NetworkType, typename NetworkInterfaceType = IConnectionService>
    class ClientAdmin final : public IClientAdmin, public AdvancedService<ClientAdmin<NetworkType, NetworkInterfaceType>> {
    public:
        ClientAdmin(Properties properties, DependencyManager *mng) : AdvancedService<ClientAdmin<NetworkType, NetworkInterfaceType>>(std::move(properties), mng), _connections{} {  }
        ~ClientAdmin() override = default;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final {
            _trackerRegistration = AdvancedService<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager().template registerDependencyTracker<NetworkInterfaceType>(this);
            _unrecoverableErrorRegistration = AdvancedService<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager().template registerEventHandler<UnrecoverableErrorEvent>(this);

            co_return {};
        }

        Task<void> stop() final {
            _trackerRegistration.reset();
            _unrecoverableErrorRegistration.reset();

            co_return;
        }

        void handleDependencyRequest(NetworkInterfaceType*, DependencyRequestEvent const &evt) {
            if(!evt.properties.has_value()) {
                throw std::runtime_error("Missing properties");
            }

            if(!evt.properties.value()->contains("Address")) {
                throw std::runtime_error("Missing address");
            }

            if(!evt.properties.value()->contains("Port")) {
                throw std::runtime_error("Missing port");
            }

            if(!_connections.contains(evt.originatingService)) {
                auto newProps = *evt.properties.value();
                newProps.emplace("Filter", Ichor::make_any<Filter>(ServiceIdFilterEntry{evt.originatingService}));

                _connections.emplace(evt.originatingService, AdvancedService<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager().template createServiceManager<NetworkType, NetworkInterfaceType>(std::move(newProps)));
            }
        }

        void handleDependencyUndoRequest(NetworkInterfaceType*, DependencyUndoRequestEvent const &evt) {
            auto connection = _connections.find(evt.originatingService);

            if(connection != end(_connections)) {
                // TODO: turn into async and await a stop service before calling remove. Current connections already are async, which will lead to the remove service event being ignored.
                AdvancedService<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager().template pushEvent<StopServiceEvent>(AdvancedService<ClientAdmin<NetworkType, NetworkInterfaceType>>::getServiceId(), connection->second->getServiceId());
                // + 11 because the first stop triggers a dep offline event and inserts a new stop with 10 higher priority.
                AdvancedService<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager().template pushPrioritisedEvent<RemoveServiceEvent>(AdvancedService<ClientAdmin<NetworkType, NetworkInterfaceType>>::getServiceId(), INTERNAL_EVENT_PRIORITY + 11, connection->second->getServiceId());
                _connections.erase(connection);
            }
        }

        AsyncGenerator<IchorBehaviour> handleEvent(UnrecoverableErrorEvent const &evt) {
            for(auto &[key, service] : _connections) {
                if(service->getServiceId() != evt.originatingService) {
                    continue;
                }

                auto const address = service->getProperties().find("Address");
                auto const port = service->getProperties().find("Port");
                auto full_address = Ichor::any_cast<std::string>(address->second);
                if(port != cend(service->getProperties())) {
                    full_address += ":" + std::to_string(Ichor::any_cast<uint16_t>(port->second));
                }
                std::string_view implNameRequestor = AdvancedService<ClientAdmin<NetworkType, NetworkInterfaceType>>::getManager().getImplementationNameFor(evt.originatingService).value();
                ICHOR_LOG_ERROR(_logger, "Couldn't start connection of type {} on address {} for service of type {} with id {} because \"{}\"", typeNameHash<NetworkType>(), full_address, implNameRequestor, service->getServiceId(), evt.error);

                break;
            }

            co_return {};
        }

        friend DependencyRegister;
        friend DependencyManager;

        ILogger *_logger{nullptr};
        unordered_map<uint64_t, IService*> _connections;
        DependencyTrackerRegistration _trackerRegistration{};
        EventHandlerRegistration _unrecoverableErrorRegistration{};
    };
}
