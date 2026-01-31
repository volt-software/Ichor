#pragma once

#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/network/IClientFactory.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/DependencyManager.h>
#include <ichor/Filter.h>
#include <ichor/ScopedServiceProxy.h>

namespace Ichor::v1 {
    template <typename NetworkType, typename NetworkInterfaceType = IConnectionService>
    class ClientFactory final : public IClientFactory<NetworkInterfaceType>, public AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>> {
    public:
        ClientFactory(DependencyRegister &reg, Properties properties) : AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>>(std::move(properties)) {
            reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED, AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>>::getProperties());
        }
        ~ClientFactory() final = default;

        ConnectionIdType createNewConnection(NeverNull<IService*> requestingSvc, Properties properties) final {
            properties.erase("Filter");
            properties.emplace("Filter", Ichor::v1::make_any<Filter>(ServiceIdFilterEntry{requestingSvc->getServiceId()}));
            ConnectionIdType count = ++_connectionCounter;

            ICHOR_LOG_TRACE(_logger, "Creating new connection {}", count.value);

            auto existingConnections = _connections.find(requestingSvc->getServiceId());

            if(existingConnections == _connections.end()) {
                unordered_map<ConnectionIdType, ServiceIdType, ConnectionIdHash> newMap;
                newMap.emplace(count, GetThreadLocalManager().template createServiceManager<NetworkType, NetworkInterfaceType>(std::move(properties), requestingSvc->getServicePriority())->getServiceId());
                _connections.emplace(requestingSvc->getServiceId(), std::move(newMap));
                return count;
            }

            existingConnections->second.emplace(count, GetThreadLocalManager().template createServiceManager<NetworkType, NetworkInterfaceType>(std::move(properties), requestingSvc->getServicePriority())->getServiceId());
            return count;
        }

        void removeConnection(NeverNull<IService*> requestingSvc, ConnectionIdType connectionId) final {
            ICHOR_LOG_TRACE(_logger, "Removing connection {}", connectionId.value);
            auto existingConnections = _connections.find(requestingSvc->getServiceId());

            if(existingConnections != end(_connections)) {
                auto connection = existingConnections->second.find(connectionId);

                if(connection == end(existingConnections->second)) {
                    ICHOR_LOG_TRACE(_logger, "Connection {} not found", connectionId.value);
                    return;
                }

                GetThreadLocalEventQueue().template pushEvent<StopServiceEvent>(AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>>::getServiceId(), connection->second, true);
                existingConnections->second.erase(connection);

                if(existingConnections->second.empty()) {
                    _connections.erase(existingConnections);
                }
            }

            ICHOR_LOG_TRACE(_logger, "Connection {} not found", connectionId.value);
        }

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final {
            _trackerRegistration = GetThreadLocalManager().template registerDependencyTracker<NetworkInterfaceType>(this, this);

            co_return {};
        }

        Task<void> stop() final {
            _trackerRegistration.reset();

            co_return;
        }

        AsyncGenerator<IchorBehaviour> handleDependencyRequest(v1::AlwaysNull<NetworkInterfaceType*>, DependencyRequestEvent const &evt) {
            if(!evt.properties.has_value()) {
                ICHOR_LOG_TRACE(_logger, "Missing properties when creating new connection {}", evt.originatingService);
                co_return {};
            }

            if(!evt.properties.value()->contains("Address")) {
                ICHOR_LOG_TRACE(_logger, "Missing address when creating new connection {}", evt.originatingService);
                co_return {};
            }

            if(!evt.properties.value()->contains("Port")) {
                ICHOR_LOG_TRACE(_logger, "Missing port when creating new connection {}", evt.originatingService);
                co_return {};
            }

            if(!_connections.contains(evt.originatingService)) {
                fmt::println("{} creating {} for {}", typeName<ClientFactory<NetworkType, NetworkInterfaceType>>(), typeName<NetworkInterfaceType>(), evt.originatingService);
                auto newProps = *evt.properties.value();
                newProps.erase("Filter");
                newProps.emplace("Filter", Ichor::v1::make_any<Filter>(ServiceIdFilterEntry{evt.originatingService}));

                unordered_map<ConnectionIdType, ServiceIdType, ConnectionIdHash> newMap;
                newMap.emplace(_connectionCounter++, GetThreadLocalManager().template createServiceManager<NetworkType, NetworkInterfaceType>(std::move(newProps), evt.priority)->getServiceId());
                _connections.emplace(evt.originatingService, std::move(newMap));
            }

            co_return {};
        }

        AsyncGenerator<IchorBehaviour> handleDependencyUndoRequest(v1::AlwaysNull<NetworkInterfaceType*>, DependencyUndoRequestEvent const &evt) {
            auto existingConnections = _connections.find(evt.originatingService);

            if(existingConnections != end(_connections)) {
                for(auto &[connectionCounter, connection] : existingConnections->second) {
                    GetThreadLocalEventQueue().template pushPrioritisedEvent<StopServiceEvent>(AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>>::getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, connection, true);
                }
                _connections.erase(existingConnections);
            }

            co_return {};
        }

        void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &) {
            _logger = std::move(logger);

        }
        void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*>, IService &) {
            _logger.reset();
        }

        friend DependencyRegister;
        friend DependencyManager;

        Ichor::ScopedServiceProxy<ILogger*> _logger {};
        ConnectionIdType _connectionCounter{};
        unordered_map<ServiceIdType, unordered_map<ConnectionIdType, ServiceIdType, ConnectionIdHash>, ServiceIdHash> _connections{};
        DependencyTrackerRegistration _trackerRegistration{};
    };
}
