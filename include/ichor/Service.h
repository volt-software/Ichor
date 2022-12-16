#pragma once

#include <cstdint>
#include <atomic>
#include <sole/sole.h>
#include <ichor/Common.h>
#include <ichor/Concepts.h>
#include <ichor/Enums.h>
#include <ichor/coroutines/AsyncGenerator.h>
#include <ichor/dependency_management/DependencyInfo.h>

namespace Ichor {

    class DependencyManager;

    template <typename T>
    class Service;

    template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
    requires DerivedTemplated<ServiceType, Service>
#endif
    class LifecycleManager;
    template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
    requires DerivedTemplated<ServiceType, Service>
#endif
    class DependencyLifecycleManager;

    class IService {
    public:
        virtual ~IService() = default;

        /// Process-local unique service id
        /// \return id
        [[nodiscard]] virtual uint64_t getServiceId() const noexcept = 0;

        /// Global unique service id
        /// \return gid
        [[nodiscard]] virtual sole::uuid getServiceGid() const noexcept = 0;

        /// Name of the user-specified service (e.g. CoutFrameworkLogger)
        /// \return
        [[nodiscard]] virtual std::string_view getServiceName() const noexcept = 0;

        [[nodiscard]] virtual ServiceState getServiceState() const noexcept = 0;

        [[nodiscard]] virtual uint64_t getServicePriority() const noexcept = 0;

        [[nodiscard]] virtual bool isStarted() const noexcept = 0;

        [[nodiscard]] virtual DependencyManager& getManager() const noexcept = 0;

        [[nodiscard]] virtual Properties& getProperties() noexcept = 0;
        [[nodiscard]] virtual const Properties& getProperties() const noexcept = 0;
    };


    extern std::atomic<uint64_t> _serviceIdCounter;

    template <typename T>
    class Service : public IService {
    public:
        template <typename U = T> requires (!RequestsProperties<U> && !RequestsDependencies<U>)
        Service() noexcept : IService(), _serviceId(_serviceIdCounter.fetch_add(1, std::memory_order_acq_rel)), _servicePriority(INTERNAL_EVENT_PRIORITY), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED) {

        }

        template <typename U = T> requires (RequestsProperties<U> || RequestsDependencies<U>)
        Service(Properties&& props, DependencyManager *mng) noexcept : IService(), _properties(std::forward<Properties>(props)), _serviceId(_serviceIdCounter.fetch_add(1, std::memory_order_acq_rel)), _servicePriority(INTERNAL_EVENT_PRIORITY), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED), _manager(mng) {

        }

        ~Service() noexcept override {
            _serviceId = 0;
            _serviceState = ServiceState::UNINSTALLED;
        }

        Service(const Service&) = default;
        Service(Service&&) noexcept = default;
        Service& operator=(const Service&) = default;
        Service& operator=(Service&&) noexcept = default;

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
        [[nodiscard]] virtual AsyncGenerator<void> start() {
            co_return;
        }

        [[nodiscard]] virtual AsyncGenerator<void> stop() {
            co_return;
        }

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
            auto gen = start();
            auto it = gen.begin();
            co_await it;

#ifdef ICHOR_USE_HARDENING
            if(!it.get_finished()) [[unlikely]] {
                std::terminate();
            }
#endif

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
            auto gen = stop();
            auto it = gen.begin();
            co_await it;

#ifdef ICHOR_USE_HARDENING
            if(!it.get_finished()) [[unlikely]] {
                std::terminate();
            }
#endif

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
            _properties = std::forward<Properties>(properties);
        }

        void injectDependencyManager(DependencyManager *mng) noexcept {
            _manager = mng;
        }

        void injectPriority(uint64_t priority) noexcept {
            _servicePriority = priority;
        }


        uint64_t _serviceId;
        uint64_t _servicePriority;
        sole::uuid _serviceGid;
        ServiceState _serviceState;
        DependencyManager *_manager{nullptr};

        friend class DependencyManager;

        template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires DerivedTemplated<ServiceType, Service>
#endif
        friend class LifecycleManager;

        template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires DerivedTemplated<ServiceType, Service>
#endif
        friend class DependencyLifecycleManager;

    };
}

#ifndef ICHOR_DEPENDENCY_MANAGER
#include <ichor/DependencyManager.h>
#endif
