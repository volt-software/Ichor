#pragma once

#include <ichor/Concepts.h>
#include <ichor/coroutines/AsyncGenerator.h>
#include <ichor/dependency_management/DependencyInfo.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/dependency_management/IService.h>
#include <utility>
#include <variant>

namespace Ichor {

    class DependencyManager;

    template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
    requires Derived<ServiceType, IService>
#endif
    class DependencyLifecycleManager;

    extern std::atomic<uint64_t> _serviceIdCounter;

    template <typename T, typename... Deps>
    class ConstructorInjectionService : public IService {
    public:
        ConstructorInjectionService(DependencyRegister &reg, Properties props, DependencyManager *mng) noexcept : IService(), _properties(std::move(props)), _serviceId(_serviceIdCounter.fetch_add(1, std::memory_order_acq_rel)), _servicePriority(INTERNAL_EVENT_PRIORITY), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED), _manager(mng) {
            (reg.registerDependencyConstructor<Deps>(this), ...);
        }

        ~ConstructorInjectionService() noexcept override {
            _serviceId = 0;
            _serviceState = ServiceState::UNINSTALLED;
        }

        ConstructorInjectionService(const ConstructorInjectionService&) = default;
        ConstructorInjectionService(ConstructorInjectionService&&) noexcept = default;
        ConstructorInjectionService& operator=(const ConstructorInjectionService&) = default;
        ConstructorInjectionService& operator=(ConstructorInjectionService&&) noexcept = default;

        /// Process-local unique service id
        /// \return id
        [[nodiscard]] uint64_t getServiceId() const noexcept final {
            return _serviceId;
        }

        /// Global unique service id
        /// \return gid
        [[nodiscard]] sole::uuid getServiceGid() const noexcept final {
            return _serviceGid;
        }

        /// Name of the user-specified service (e.g. CoutFrameworkLogger)
        /// \return
        [[nodiscard]] std::string_view getServiceName() const noexcept final {
            return typeName<T>();
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _serviceState;
        }

        [[nodiscard]] uint64_t getServicePriority() const noexcept final {
            return _servicePriority;
        }

        [[nodiscard]] bool isStarted() const noexcept final {
            return _serviceState == ServiceState::ACTIVE;
        }

        [[nodiscard]] DependencyManager& getManager() const noexcept final {
            return *_manager;
        }

        [[nodiscard]] Properties& getProperties() noexcept final {
            return _properties;
        }

        [[nodiscard]] const Properties& getProperties() const noexcept final {
            return _properties;
        }

    protected:
        Properties _properties;
    private:
        ///
        /// \return true if started
        [[nodiscard]] AsyncGenerator<StartBehaviour> internal_start(DependencyInfo *_dependencies) {
            if(_serviceState != ServiceState::INSTALLED || (_dependencies != nullptr && !_dependencies->allSatisfied())) {
                INTERNAL_DEBUG("internal_start service {}:{} state {} dependencies {} {}", getServiceId(), typeName<T>(), getState(), _dependencies->size(), _dependencies->allSatisfied());
                co_return {};
            }

            INTERNAL_DEBUG("internal_start service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::STARTING);
            _serviceState = ServiceState::STARTING;
            new (buf) T(static_cast<IService*>(this), std::get<Deps*...>(_deps[typeNameHash<Deps...>()].first));

            INTERNAL_DEBUG("internal_start service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::INJECTING);
            _serviceState = ServiceState::INJECTING;

            co_return {};
        }
        ///
        /// \return true if stopped or already stopped
        [[nodiscard]] AsyncGenerator<StartBehaviour> internal_stop() {
#ifdef ICHOR_USE_HARDENING
            if(_serviceState != ServiceState::UNINJECTING) [[unlikely]] {
                std::terminate();
            }
#endif

            INTERNAL_DEBUG("internal_stop service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::STOPPING);
            _serviceState = ServiceState::STOPPING;
            reinterpret_cast<T*>(buf)->~T();

            INTERNAL_DEBUG("internal_stop service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::INSTALLED);
            _serviceState = ServiceState::INSTALLED;

            co_return {};
        }

        [[nodiscard]] bool internalSetInjected() {
            if(_serviceState != ServiceState::INJECTING) {
                return false;
            }

            INTERNAL_DEBUG("internalSetInjected service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::ACTIVE);
            _serviceState = ServiceState::ACTIVE;
            return true;
        }

        [[nodiscard]] bool internalSetUninjected() {
            if(_serviceState != ServiceState::ACTIVE) {
                return false;
            }

            INTERNAL_DEBUG("internalSetUninjected service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::UNINJECTING);
            _serviceState = ServiceState::UNINJECTING;
            return true;
        }

        [[nodiscard]] ServiceState getState() const noexcept {
            return _serviceState;
        }

        void setProperties(Properties&& properties) noexcept(std::is_nothrow_move_assignable_v<Properties>) {
            _properties = std::move(properties);
        }

        void injectDependencyManager(DependencyManager *mng) noexcept {
            _manager = mng;
        }

        void injectPriority(uint64_t priority) noexcept {
            _servicePriority = priority;
        }

        template <typename depT>
        void addDependencyInstance(depT* dep, IService *svc) {
            _deps.emplace(typeNameHash<depT>(), std::make_pair<std::variant<Deps*...>, IService*>(dep, std::forward<IService*>(svc)));
        }

        template <typename depT>
        void removeDependencyInstance(depT*, IService *) {
            _deps.erase(typeNameHash<depT>());
        }


        uint64_t _serviceId;
        uint64_t _servicePriority;
        sole::uuid _serviceGid;
        ServiceState _serviceState;
        DependencyManager *_manager{nullptr};
        unordered_map<uint64_t, std::pair<std::variant<Deps*...>, IService*>> _deps{};
        alignas(T) std::byte buf[sizeof(T)];

        friend class DependencyManager;
        friend struct DependencyRegister;

        template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<ServiceType, IService>
#endif
        friend class DependencyLifecycleManager;

    };
}

#ifndef ICHOR_DEPENDENCY_MANAGER
#include <ichor/DependencyManager.h>
#endif
