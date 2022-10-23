#pragma once

#include <cstdint>
#include <atomic>
#include "sole.hpp"
#include <ichor/Common.h>
#include <ichor/Concepts.h>
#include <ichor/Enums.h>

namespace Ichor {
    class DependencyManager;
    template <typename T>
    class Service;
    template<class ServiceType, typename... IFaces>
    requires DerivedTemplated<ServiceType, Service>
    class LifecycleManager;
    template<class ServiceType, typename... IFaces>
    requires DerivedTemplated<ServiceType, Service>
    class DependencyLifecycleManager;

    class IService {
    public:
        virtual ~IService() = default;

        [[nodiscard]] virtual uint64_t getServiceId() const noexcept = 0;

        [[nodiscard]] virtual uint64_t getServicePriority() const noexcept = 0;

        [[nodiscard]] virtual DependencyManager& getManager() const noexcept = 0;

        [[nodiscard]] virtual Properties& getProperties() noexcept = 0;
        [[nodiscard]] virtual const Properties& getProperties() const noexcept = 0;
    };


    extern std::atomic<uint64_t> _serviceIdCounter;

    template <typename T>
    class Service : public IService {
    public:
        template <typename U = T> requires (!RequestsProperties<U> && !RequestsDependencies<U>)
        Service() noexcept : IService(), _properties(), _serviceId(_serviceIdCounter.fetch_add(1, std::memory_order_acq_rel)), _servicePriority(1000), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED) {

        }

        template <typename U = T> requires (RequestsProperties<U> || RequestsDependencies<U>)
        Service(Properties&& props, DependencyManager *mng) noexcept: IService(), _properties(std::forward<Properties>(props)), _serviceId(_serviceIdCounter.fetch_add(1, std::memory_order_acq_rel)), _servicePriority(1000), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED), _manager(mng) {

        }

        ~Service() noexcept override {
            _serviceId = 0;
            _serviceGid.ab = 0;
            _serviceGid.cd = 0;
            _serviceState = ServiceState::UNINSTALLED;
        }

        Service(const Service&) = default;
        Service(Service&&) noexcept = default;
        Service& operator=(const Service&) = default;
        Service& operator=(Service&&) noexcept = default;

        [[nodiscard]] uint64_t getServiceId() const noexcept final {
            return _serviceId;
        }

        [[nodiscard]] uint64_t getServicePriority() const noexcept final {
            return _servicePriority;
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
        [[nodiscard]] virtual StartBehaviour start() {
            return StartBehaviour::SUCCEEDED;
        }

        [[nodiscard]] virtual StartBehaviour stop() {
            return StartBehaviour::SUCCEEDED;
        }

        Properties _properties;
    private:
        ///
        /// \return true if started
        [[nodiscard]] StartBehaviour internal_start() {
            if(_serviceState != ServiceState::INSTALLED && _serviceState != ServiceState::STARTING) {
                return StartBehaviour::FAILED_DO_NOT_RETRY;
            }

            INTERNAL_DEBUG("service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::STARTING);
            _serviceState = ServiceState::STARTING;
            auto ret = start();
            if(ret == StartBehaviour::SUCCEEDED) {
                INTERNAL_DEBUG("service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::INJECTING);
                _serviceState = ServiceState::INJECTING;
            }

            return ret;
        }
        ///
        /// \return true if stopped or already stopped
        [[nodiscard]] StartBehaviour internal_stop() {
            if(_serviceState != ServiceState::UNINJECTING && _serviceState != ServiceState::STOPPING) {
                return StartBehaviour::FAILED_DO_NOT_RETRY;
            }

            INTERNAL_DEBUG("service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::STOPPING);
            _serviceState = ServiceState::STOPPING;
            auto ret = stop();
            if(ret == StartBehaviour::SUCCEEDED) {
                INTERNAL_DEBUG("service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::INSTALLED);
                _serviceState = ServiceState::INSTALLED;
            }

            return ret;
        }

        [[nodiscard]] bool internalSetInjected() {
            if(_serviceState != ServiceState::INJECTING) {
                return false;
            }

            INTERNAL_DEBUG("service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::ACTIVE);
            _serviceState = ServiceState::ACTIVE;
            return true;
        }

        [[nodiscard]] bool internalSetUninjected() {
            if(_serviceState != ServiceState::ACTIVE) {
                return false;
            }

            INTERNAL_DEBUG("service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::UNINJECTING);
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
        requires DerivedTemplated<ServiceType, Service>
        friend class LifecycleManager;
        template<class ServiceType, typename... IFaces>
        requires DerivedTemplated<ServiceType, Service>
        friend class DependencyLifecycleManager;
    };
}
