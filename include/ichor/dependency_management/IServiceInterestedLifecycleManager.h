#pragma once

#define SELF_SERVICE_ID std::numeric_limits<ServiceIdType>::max();

namespace Ichor::Detail {
    /// implementation of ILifecycleManager purely to find out if another ILifecycleManager is interested in IService
    class IServiceInterestedLifecycleManager final : public ILifecycleManager {
    public:
        IServiceInterestedLifecycleManager(IService *self) : _self(self) {
            _interfaces.emplace_back(typeNameHash<IService>(), typeName<IService>(), DependencyFlags::NONE, false);
        }
        ~IServiceInterestedLifecycleManager() final = default;

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
        unordered_set<ServiceIdType, ServiceIdHash> &getDependencies() noexcept final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]]
        unordered_set<ServiceIdType, ServiceIdHash> &getDependees() noexcept final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> startAfterDependencyOnline() final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> start() final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> stop() final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]]
        bool setInjected() final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]]
        bool setUninjected() final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]] ICHOR_PURE_FUNC_ATTR std::string_view implementationName() const noexcept final {
            return typeName<IService>();
        }

        [[nodiscard]] ICHOR_PURE_FUNC_ATTR uint64_t type() const noexcept final {
            return typeNameHash<IService>();
        }

        [[nodiscard]] ICHOR_PURE_FUNC_ATTR ServiceIdType serviceId() const noexcept final {
            return SELF_SERVICE_ID;
        }

        [[nodiscard]] uint64_t getPriority() const noexcept final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]] v1::NeverNull<IService*> getIService() noexcept final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]] v1::NeverNull<IService const*> getIService() const noexcept final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]] std::span<Dependency const> getInterfaces() const noexcept final {
            return std::span<Dependency const>{_interfaces.begin(), _interfaces.size()};
        }

        [[nodiscard]] Properties const & getProperties() const noexcept final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]] DependencyRegister const * getDependencyRegistry() const noexcept final {
            // this function should never be called
            std::terminate();
        }

        /// Someone is interested in us, inject ourself into them
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
        /// \param fn
        void insertSelfInto(uint64_t keyOfInterfaceToInject, ServiceIdType serviceIdOfOther, std::function<void(v1::NeverNull<void*>, IService&)> &fn) final {
            if(keyOfInterfaceToInject != typeNameHash<IService>()) {
                return;
            }

            INTERNAL_DEBUG("insertSelfInto() svc {} telling svc {} to add us", serviceId(), serviceIdOfOther);
            fn(_self, *_self);
        }

        /// The underlying service got stopped and someone else is asking us to remove ourselves from them.
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
        /// \param fn
        void removeSelfInto(uint64_t keyOfInterfaceToInject, ServiceIdType serviceIdOfOther, std::function<void(v1::NeverNull<void*>, IService&)> &fn) final {
            if(keyOfInterfaceToInject != typeNameHash<IService>()) {
                return;
            }

            INTERNAL_DEBUG("removeSelfInto() svc {} removing svc {}", serviceId(), serviceIdOfOther);
            fn(_self, *_self);
        }

    private:
        v1::StaticVector<Dependency, 1> _interfaces;
        IService *_self;
    };
}
