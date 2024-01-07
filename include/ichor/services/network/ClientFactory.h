#pragma once

#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/network/IClientFactory.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/DependencyManager.h>
#include <thread>

namespace Ichor {
    using ConnectionCounterType = uint64_t;

    template <typename NetworkType, typename NetworkInterfaceType = IConnectionService>
    class ClientFactory final : public IClientFactory, public AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>> {
    public:
        ClientFactory(DependencyRegister &reg, Properties properties) : AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>>(std::move(properties)) {
            reg.registerDependency<ILogger>(this, DependencyFlags::NONE, AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>>::getProperties());
        }
        ~ClientFactory() final = default;

        uint64_t createNewConnection(NeverNull<IService*> requestingSvc, Properties properties) final {
            properties.emplace("Filter", Ichor::make_any<Filter>(ServiceIdFilterEntry{requestingSvc->getServiceId()}));
            ConnectionCounterType count = _connectionCounter++;

            ICHOR_LOG_TRACE(_logger, "Creating new connection {}", count);

            auto existingConnections = _connections.find(requestingSvc->getServiceId());

            if(existingConnections == _connections.end()) {
                unordered_map<ConnectionCounterType, IService*> newMap;
                newMap.emplace(count, GetThreadLocalManager().template createServiceManager<NetworkType, NetworkInterfaceType>(std::move(properties), requestingSvc->getServicePriority()));
                _connections.emplace(requestingSvc->getServiceId(), std::move(newMap));
                return count;
            }

            existingConnections->second.emplace(requestingSvc->getServiceId(), GetThreadLocalManager().template createServiceManager<NetworkType, NetworkInterfaceType>(std::move(properties), requestingSvc->getServicePriority()));
            return count;
        }

        void removeConnection(NeverNull<IService*> requestingSvc, uint64_t connectionId) final {
            ICHOR_LOG_TRACE(_logger, "Removing connection {}", connectionId);
            auto existingConnections = _connections.find(requestingSvc->getServiceId());

            if(existingConnections != end(_connections)) {
                auto connection = existingConnections->second.find(connectionId);

                if(connection == end(existingConnections->second)) {
                    ICHOR_LOG_TRACE(_logger, "Connection {} not found", connectionId);
                    return;
                }

                // TODO: turn into async and await a stop service before calling remove. Current connections already are async, which will lead to the remove service event being ignored.
                GetThreadLocalEventQueue().template pushEvent<StopServiceEvent>(AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>>::getServiceId(), connection->second->getServiceId());
                // + 11 because the first stop triggers a dep offline event and inserts a new stop with 10 higher priority.
                GetThreadLocalEventQueue().template pushPrioritisedEvent<RemoveServiceEvent>(AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>>::getServiceId(), INTERNAL_EVENT_PRIORITY + 11, connection->second->getServiceId());
                existingConnections->second.erase(connection);

                if(existingConnections->second.empty()) {
                    _connections.erase(existingConnections);
                }
            }

            ICHOR_LOG_TRACE(_logger, "Connection {} not found", connectionId);
        }

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final {
            _trackerRegistration = GetThreadLocalManager().template registerDependencyTracker<NetworkInterfaceType>(this, this);
            _unrecoverableErrorRegistration = GetThreadLocalManager().template registerEventHandler<UnrecoverableErrorEvent>(this, this);

            co_return {};
        }

        Task<void> stop() final {
            _trackerRegistration.reset();
            _unrecoverableErrorRegistration.reset();

            co_return;
        }

        void handleDependencyRequest(AlwaysNull<NetworkInterfaceType*>, DependencyRequestEvent const &evt) {
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

                unordered_map<ConnectionCounterType, IService*> newMap;
                newMap.emplace(_connectionCounter++, GetThreadLocalManager().template createServiceManager<NetworkType, NetworkInterfaceType>(std::move(newProps), evt.priority));
                _connections.emplace(evt.originatingService, std::move(newMap));
            }
        }

        void handleDependencyUndoRequest(AlwaysNull<NetworkInterfaceType*>, DependencyUndoRequestEvent const &evt) {
            auto existingConnections = _connections.find(evt.originatingService);

            if(existingConnections != end(_connections)) {
                for(auto &[connectionCounter, connection] : existingConnections->second) {
                    // TODO: turn into async and await a stop service before calling remove. Current connections already are async, which will lead to the remove service event being ignored.
                    GetThreadLocalEventQueue().template pushEvent<StopServiceEvent>(AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>>::getServiceId(), connection->getServiceId());
                    // + 11 because the first stop triggers a dep offline event and inserts a new stop with 10 higher priority.
                    GetThreadLocalEventQueue().template pushPrioritisedEvent<RemoveServiceEvent>(AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>>::getServiceId(), INTERNAL_EVENT_PRIORITY + 11, connection->getServiceId());
                }
                _connections.erase(existingConnections);
            }
        }

        AsyncGenerator<IchorBehaviour> handleEvent(UnrecoverableErrorEvent const &evt) {
            for(auto &[serviceId, map] : _connections) {
                for(auto &[connectionId, service] : map) {
                    if (service->getServiceId() != evt.originatingService) {
                        continue;
                    }

                    auto const address = service->getProperties().find("Address");
                    auto const port = service->getProperties().find("Port");
                    auto full_address = Ichor::any_cast<std::string>(address->second);
                    if (port != cend(service->getProperties())) {
                        full_address += ":" + std::to_string(Ichor::any_cast<uint16_t>(port->second));
                    }
                    std::string_view implNameRequestor = GetThreadLocalManager().getImplementationNameFor(evt.originatingService).value();
                    ICHOR_LOG_ERROR(_logger, "Couldn't start connection of type {} on address {} for service of type {} with id {} because \"{}\"",
                                    typeNameHash<NetworkType>(), full_address, implNameRequestor, service->getServiceId(), evt.error);

                    co_return {};
                }
            }

            co_return {};
        }


        void addDependencyInstance(ILogger &logger, IService &) {
            _logger = &logger;

        }
        void removeDependencyInstance(ILogger &, IService &) {
            _logger = nullptr;
        }

        friend DependencyRegister;
        friend DependencyManager;

        ILogger *_logger{};
        uint64_t _connectionCounter{};
        unordered_map<ServiceIdType, unordered_map<ConnectionCounterType, IService*>> _connections{};
        DependencyTrackerRegistration _trackerRegistration{};
        EventHandlerRegistration _unrecoverableErrorRegistration{};
    };
}
