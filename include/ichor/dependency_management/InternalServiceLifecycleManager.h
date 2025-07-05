#pragma once

#include <ichor/dependency_management/InternalService.h>
#include <ichor/stl/StaticVector.h>

namespace Ichor::Detail {
    extern thread_local unordered_set<uint64_t> emptyDependencies;

    /// InternalServiceLifecycleManager is meant to be used for Ichor internal services. This service does not actually own a service like the regular (Dependency)LifecycleManager, but "pretends" to be a LifecycleManager.
    /// This is especially useful for queues and the DM itself, as they are created outside of the regular dependency mechanism, but we still want services to request these as dependencies.
    /// \tparam ServiceType
    /// \tparam IFaces
    template<class ServiceType>
    class InternalServiceLifecycleManager final : public ILifecycleManager {
    public:
        explicit InternalServiceLifecycleManager(ServiceType *q) : _q(q) {
            _interfaces.emplace_back(typeNameHash<ServiceType>(), typeName<ServiceType>(), DependencyFlags::NONE, false);
            _service.setState(ServiceState::ACTIVE);
        }

        ~InternalServiceLifecycleManager() final = default;

        std::vector<Dependency*> interestedInDependencyGoingOffline(ILifecycleManager *dependentService) noexcept final {
            return {};
        }

        StartBehaviour dependencyOnline(v1::NeverNull<ILifecycleManager*> dependentService) final {
            return StartBehaviour::DONE;
        }

        AsyncGenerator<StartBehaviour> dependencyOffline(v1::NeverNull<ILifecycleManager*> dependentService, std::vector<Dependency*> deps) final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependencies() noexcept final {
            return emptyDependencies;
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependees() noexcept final {
            return _serviceIdsOfDependees;
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> startAfterDependencyOnline() final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> start() final {
            _service.setState(ServiceState::ACTIVE);
            co_return {};
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> stop() final {
            _service.setState(ServiceState::INSTALLED);
            co_return {};
        }

        [[nodiscard]]
        bool setInjected() final {
            if(_service.getState() != ServiceState::INJECTING) {
                return false;
            }
            _service.setState(ServiceState::ACTIVE);
            return true;
        }

        [[nodiscard]]
        bool setUninjected() final {
            if(_service.getState() != ServiceState::ACTIVE) {
                return false;
            }
            _service.setState(ServiceState::UNINJECTING);
            return true;
        }

        [[nodiscard]] std::string_view implementationName() const noexcept final {
            return typeName<ServiceType>();
        }

        [[nodiscard]] uint64_t type() const noexcept final {
            return typeNameHash<ServiceType>();
        }

        [[nodiscard]] ServiceIdType serviceId() const noexcept final {
            return _service.getServiceId();
        }

        [[nodiscard]] uint64_t getPriority() const noexcept final {
            return INTERNAL_EVENT_PRIORITY;
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _service.getServiceState();
        }

        [[nodiscard]] v1::NeverNull<IService*> getIService() noexcept final {
            return &_service;
        }

        [[nodiscard]] v1::NeverNull<IService const*> getIService() const noexcept final {
            return &_service;
        }

        [[nodiscard]] std::span<Dependency const> getInterfaces() const noexcept final {
            return std::span<Dependency const>{_interfaces.begin(), _interfaces.size()};
        }

        [[nodiscard]] Properties const & getProperties() const noexcept final {
            return _service.getProperties();
        }

        [[nodiscard]] DependencyRegister const * getDependencyRegistry() const noexcept final {
            return nullptr;
        }

        /// Someone is interested in us, inject ourself into them
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
        /// \param fn
        void insertSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(v1::NeverNull<void*>, IService&)> &fn) final {
            if(keyOfInterfaceToInject != typeNameHash<ServiceType>()) {
                return;
            }

            INTERNAL_DEBUG("insertSelfInto() svc {} telling svc {} to add us", serviceId(), serviceIdOfOther);
            fn(_q, _service);
            _serviceIdsOfDependees.insert(serviceIdOfOther);
        }

        /// The underlying service got stopped and someone else is asking us to remove ourselves from them.
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
        /// \param fn
        void removeSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(v1::NeverNull<void*>, IService&)> &fn) final {
            if(keyOfInterfaceToInject != typeNameHash<ServiceType>()) {
                return;
            }

            INTERNAL_DEBUG("removeSelfInto() svc {} removing svc {}", serviceId(), serviceIdOfOther);
            fn(_q, _service);
            _serviceIdsOfDependees.erase(serviceIdOfOther);
        }

    private:
        ServiceType *_q;
        unordered_set<uint64_t> _serviceIdsOfDependees; // services that depend on this service
        v1::StaticVector<Dependency, 1> _interfaces;
        InternalService<ServiceType> _service;
    };
}
