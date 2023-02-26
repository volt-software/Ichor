#pragma once

#include <ichor/Concepts.h>
#include <ichor/coroutines/Task.h>
#include <ichor/dependency_management/DependencyInfo.h>
#include <ichor/dependency_management/IService.h>
#include <tl/expected.h>
#include <atomic>

namespace Ichor {
    template <typename T>
    class AdvancedService;
    class DependencyManager;

    template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
    requires DerivedTemplated<ServiceType, AdvancedService> || IsConstructorInjector<ServiceType>
#endif
    class LifecycleManager;
    template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
    requires Derived<ServiceType, IService>
#endif
    class DependencyLifecycleManager;

    extern std::atomic<uint64_t> _serviceIdCounter;

    template <typename T>
    class AdvancedService : public IService {
    public:
        template <typename U = T> requires (!RequestsProperties<U> && !RequestsDependencies<U>)
        AdvancedService() noexcept : IService(), _serviceId(_serviceIdCounter.fetch_add(1, std::memory_order_relaxed)), _servicePriority(INTERNAL_EVENT_PRIORITY), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED) {

        }

        template <typename U = T> requires (RequestsProperties<U> || RequestsDependencies<U>)
        AdvancedService(Properties&& props) noexcept : IService(), _properties(std::move(props)), _serviceId(_serviceIdCounter.fetch_add(1, std::memory_order_relaxed)), _servicePriority(INTERNAL_EVENT_PRIORITY), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED) {

        }

        ~AdvancedService() noexcept override {
            _serviceId = 0;
            _serviceState = ServiceState::UNINSTALLED;
        }

        AdvancedService(const AdvancedService&) = default;
        AdvancedService(AdvancedService&&) noexcept = default;
        AdvancedService& operator=(const AdvancedService&) = default;
        AdvancedService& operator=(AdvancedService&&) noexcept = default;

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

        void setServicePriority(uint64_t priority) noexcept final {
            _servicePriority = priority;
        }

        [[nodiscard]] bool isStarted() const noexcept final {
            return _serviceState == ServiceState::ACTIVE;
        }

        [[nodiscard]] Properties& getProperties() noexcept final {
            return _properties;
        }

        [[nodiscard]] const Properties& getProperties() const noexcept final {
            return _properties;
        }

        [[nodiscard]] void const * getTypedServicePtr() const noexcept {
            return this;
        }

    protected:
        [[nodiscard]] virtual Task<tl::expected<void, StartError>> start() {
            co_return {};
        }

        [[nodiscard]] virtual Task<void> stop() {
            co_return;
        }

        Properties _properties;
    private:
        ///
        /// \return true if started
        [[nodiscard]] Task<StartBehaviour> internal_start(DependencyInfo *_dependencies) {
            if(_serviceState != ServiceState::INSTALLED || (_dependencies != nullptr && !_dependencies->allSatisfied())) {
                INTERNAL_DEBUG("internal_start service {}:{} state {} dependencies {} {}", getServiceId(), typeName<T>(), getState(), _dependencies != nullptr ? _dependencies->size() : (size_t)-1, _dependencies != nullptr ? _dependencies->allSatisfied() : false);
                co_return {};
            }

            INTERNAL_DEBUG("internal_start service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::STARTING);
            _serviceState = ServiceState::STARTING;
            tl::expected<void, StartError> outcome = co_await start();

            if(!outcome) {
                INTERNAL_DEBUG("internal_start service {}:{} state {} error starting", getServiceId(), typeName<T>(), getState());
                _serviceState = ServiceState::INSTALLED;
                co_return StartBehaviour::STOPPED;
            }

            INTERNAL_DEBUG("internal_start service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::INJECTING);
            _serviceState = ServiceState::INJECTING;

            co_return {};
        }
        ///
        /// \return true if stopped or already stopped
        [[nodiscard]] Task<StartBehaviour> internal_stop() {
#ifdef ICHOR_USE_HARDENING
            if(_serviceState != ServiceState::UNINJECTING) [[unlikely]] {
                std::terminate();
            }
#endif

            INTERNAL_DEBUG("internal_stop service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::STOPPING);
            _serviceState = ServiceState::STOPPING;
            co_await stop();

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

        [[nodiscard]] T* getImplementation() noexcept {
            return static_cast<T*>(this);
        }

        uint64_t _serviceId;
        uint64_t _servicePriority;
        sole::uuid _serviceGid;
        ServiceState _serviceState;

        template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires DerivedTemplated<ServiceType, AdvancedService> || IsConstructorInjector<ServiceType>
#endif
        friend class LifecycleManager;

        template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<ServiceType, IService>
#endif
        friend class DependencyLifecycleManager;

        friend class DependencyManager;
    };
}
