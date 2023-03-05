#pragma once

#define SELF_SERVICE_ID std::numeric_limits<uint64_t>::max();

namespace Ichor::Detail {
    /// implementation of ILifecycleManager purely to find out if another ILifecycleManager is interested in IService
    class IServiceInterestedLifecycleManager final : public ILifecycleManager {
    public:
        IServiceInterestedLifecycleManager(IService *self) : _self(self) {
            _interfaces.emplace_back(typeNameHash<IService>(), false, false);
        }
        ~IServiceInterestedLifecycleManager() final = default;

        std::vector<decltype(std::declval<DependencyInfo>().begin())> interestedInDependency(ILifecycleManager *, bool) noexcept final {
            // this function should never be called
            std::terminate();
        }

        AsyncGenerator<StartBehaviour> dependencyOnline(ILifecycleManager* dependentService, std::vector<decltype(std::declval<DependencyInfo>().begin())> iterators) final {
            // this function should never be called
            std::terminate();
        }

        AsyncGenerator<StartBehaviour> dependencyOffline(ILifecycleManager* dependentService, std::vector<decltype(std::declval<DependencyInfo>().begin())> iterators) final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependencies() noexcept final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependees() noexcept final {
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

        [[nodiscard]] std::string_view implementationName() const noexcept final {
            return typeName<IService>();
        }

        [[nodiscard]] uint64_t type() const noexcept final {
            return typeNameHash<IService>();
        }

        [[nodiscard]] uint64_t serviceId() const noexcept final {
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

        [[nodiscard]] IService * getIService() noexcept final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]] IService const * getIService() const noexcept final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]] const std::vector<Dependency>& getInterfaces() const noexcept final {
            return _interfaces;
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
        void insertSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)> &fn) final {
            if(keyOfInterfaceToInject != typeNameHash<IService>()) {
                return;
            }

            INTERNAL_DEBUG("insertSelfInto() svc {} telling svc {} to add us", serviceId(), serviceIdOfOther);
            fn(_self, _self);
        }

        /// The underlying service got stopped and someone else is asking us to remove ourselves from them.
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
        /// \param fn
        void removeSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)> &fn) final {
            if(keyOfInterfaceToInject != typeNameHash<IService>()) {
                return;
            }

            INTERNAL_DEBUG("removeSelfInto() svc {} removing svc {}", serviceId(), serviceIdOfOther);
            fn(_self, _self);
        }

    private:
        std::vector<Dependency> _interfaces;
        IService *_self;
    };
}
