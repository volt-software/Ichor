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
                unordered_map<ConnectionCounterType, ServiceIdType> newMap;
                newMap.emplace(count, GetThreadLocalManager().template createServiceManager<NetworkType, NetworkInterfaceType>(std::move(properties), requestingSvc->getServicePriority())->getServiceId());
                _connections.emplace(requestingSvc->getServiceId(), std::move(newMap));
                return count;
            }

            existingConnections->second.emplace(requestingSvc->getServiceId(), GetThreadLocalManager().template createServiceManager<NetworkType, NetworkInterfaceType>(std::move(properties), requestingSvc->getServicePriority())->getServiceId());
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

                GetThreadLocalEventQueue().template pushEvent<StopServiceEvent>(AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>>::getServiceId(), connection->second, true);
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

            co_return {};
        }

        Task<void> stop() final {
            _trackerRegistration.reset();

            co_return;
        }

        AsyncGenerator<IchorBehaviour> handleDependencyRequest(AlwaysNull<NetworkInterfaceType*>, DependencyRequestEvent const &evt) {
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

                unordered_map<ConnectionCounterType, ServiceIdType> newMap;
                newMap.emplace(_connectionCounter++, GetThreadLocalManager().template createServiceManager<NetworkType, NetworkInterfaceType>(std::move(newProps), evt.priority)->getServiceId());
                _connections.emplace(evt.originatingService, std::move(newMap));
            }

            co_return {};
        }

        AsyncGenerator<IchorBehaviour> handleDependencyUndoRequest(AlwaysNull<NetworkInterfaceType*>, DependencyUndoRequestEvent const &evt) {
            auto existingConnections = _connections.find(evt.originatingService);

            if(existingConnections != end(_connections)) {
                for(auto &[connectionCounter, connection] : existingConnections->second) {
                    GetThreadLocalEventQueue().template pushPrioritisedEvent<StopServiceEvent>(AdvancedService<ClientFactory<NetworkType, NetworkInterfaceType>>::getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, connection, true);
                }
                _connections.erase(existingConnections);
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
        unordered_map<ServiceIdType, unordered_map<ConnectionCounterType, ServiceIdType>> _connections{};
        DependencyTrackerRegistration _trackerRegistration{};
    };
}
