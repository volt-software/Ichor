#pragma once

#include <ichor/dependency_management/InternalService.h>

namespace Ichor::Detail {
    /// This lifecycle manager is only for adding the queue as a service
    /// \tparam ServiceType
    /// \tparam IFaces
    class QueueLifecycleManager final : public ILifecycleManager {
    public:
        explicit QueueLifecycleManager(IEventQueue *q) : _q(q) {
            _interfaces.emplace_back(typeNameHash<IEventQueue>(), typeName<IEventQueue>(), DependencyFlags::NONE, false);
        }

        ~QueueLifecycleManager() final = default;

        std::vector<Dependency*> interestedInDependency(ILifecycleManager *dependentService, bool online) noexcept final {
            return {};
        }

        AsyncGenerator<StartBehaviour> dependencyOnline(NeverNull<ILifecycleManager*> dependentService, std::vector<Dependency*> deps) final {
            // this function should never be called
            std::terminate();
            co_return StartBehaviour::DONE;
        }

        AsyncGenerator<StartBehaviour> dependencyOffline(NeverNull<ILifecycleManager*> dependentService, std::vector<Dependency*> deps) final {
            // this function should never be called
            std::terminate();
            co_return StartBehaviour::DONE;
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
        AsyncGenerator<StartBehaviour> start() final {
            co_return {};
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> stop() final {
            _state = ServiceState::INSTALLED;
            co_return {};
        }

        [[nodiscard]]
        bool setInjected() final {
            return true;
        }

        [[nodiscard]]
        bool setUninjected() final {
            _state = ServiceState::UNINJECTING;
            return true;
        }

        [[nodiscard]] std::string_view implementationName() const noexcept final {
            return "IEventQueue";
        }

        [[nodiscard]] uint64_t type() const noexcept final {
            return typeNameHash<IEventQueue>();
        }

        [[nodiscard]] ServiceIdType serviceId() const noexcept final {
            return _service.getServiceId();
        }

        [[nodiscard]] uint64_t getPriority() const noexcept final {
            return INTERNAL_EVENT_PRIORITY;
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _state;
        }

        [[nodiscard]] NeverNull<IService*> getIService() noexcept final {
            return &_service;
        }

        [[nodiscard]] NeverNull<IService const*> getIService() const noexcept final {
            return &_service;
        }

        [[nodiscard]] const std::vector<Dependency>& getInterfaces() const noexcept final {
            return _interfaces;
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
        void insertSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(NeverNull<void*>, IService&)> &fn) final {
            if(keyOfInterfaceToInject != typeNameHash<IEventQueue>()) {
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
        void removeSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(NeverNull<void*>, IService&)> &fn) final {
            if(keyOfInterfaceToInject != typeNameHash<IEventQueue>()) {
                return;
            }

            INTERNAL_DEBUG("removeSelfInto() svc {} removing svc {}", serviceId(), serviceIdOfOther);
            fn(_q, _service);
            _serviceIdsOfDependees.erase(serviceIdOfOther);
        }

    private:
        IEventQueue *_q;
        ServiceState _state{ServiceState::ACTIVE};
        unordered_set<uint64_t> _serviceIdsOfDependees; // services that depend on this service
        std::vector<Dependency> _interfaces;
        InternalService<IEventQueue> _service;
    };
}
