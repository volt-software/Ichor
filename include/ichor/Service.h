#pragma once

#include <cstdint>
#include <atomic>
#include "sole.hpp"
#include <ichor/Common.h>
#include <ichor/Concepts.h>

namespace Ichor {
    class DependencyManager;

    enum class ServiceState {
        UNINSTALLED,
        INSTALLED,
        RESOLVED,
        STARTING,
        STOPPING,
        ACTIVE,
        UNKNOWN
    };

    class IService {
    public:
        virtual ~IService() = default;

        [[nodiscard]] virtual uint64_t getServiceId() const noexcept = 0;

        [[nodiscard]] virtual uint64_t getServicePriority() const noexcept = 0;

        [[nodiscard]] virtual DependencyManager* getManager() noexcept = 0;

        [[nodiscard]] virtual IchorProperties* getProperties() noexcept = 0;

        [[nodiscard]] std::pmr::memory_resource* getMemoryResource() noexcept;
    };


    extern std::atomic<uint64_t> _serviceIdCounter;

    template <typename T>
    class Service : virtual public IService {
    public:
        template <typename U = T> requires (!RequestsProperties<U> && !RequestsDependencies<U>)
        Service() noexcept : IService(), _properties(), _serviceId(_serviceIdCounter.fetch_add(1, std::memory_order_acq_rel)), _servicePriority(1000), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED) {

        }

        template <typename U = T> requires (RequestsProperties<U> || RequestsDependencies<U>)
        Service(IchorProperties&& props, DependencyManager *mng) noexcept: IService(), _properties(std::forward<IchorProperties>(props)), _serviceId(_serviceIdCounter.fetch_add(1, std::memory_order_acq_rel)), _servicePriority(1000), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED), _manager(mng) {

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

        [[nodiscard]] DependencyManager* getManager() noexcept final {
            return _manager;
        }

        [[nodiscard]] IchorProperties* getProperties() noexcept final {
            return &_properties;
        }

    protected:
        [[nodiscard]] virtual bool start() {
            return true;
        }

        [[nodiscard]] virtual bool stop() {
            return true;
        }

        IchorProperties _properties;
    private:
        ///
        /// \return true if started
        [[nodiscard]] bool internal_start() {
            if(_serviceState != ServiceState::INSTALLED) {
                return false;
            }

            _serviceState = ServiceState::STARTING;
            if(start()) {
                _serviceState = ServiceState::ACTIVE;
                return true;
            } else {
                _serviceState = ServiceState::INSTALLED;
            }

            return false;
        }
        ///
        /// \return true if stopped or already stopped
        [[nodiscard]] bool internal_stop() {
            if(_serviceState != ServiceState::ACTIVE) {
                return false;
            }

            _serviceState = ServiceState::STOPPING;
            if(stop()) {
                _serviceState = ServiceState::INSTALLED;
                return true;
            } else {
                _serviceState = ServiceState::UNKNOWN;
            }

            return false;
        }

        [[nodiscard]] ServiceState getState() const noexcept {
            return _serviceState;
        }

        void setProperties(IchorProperties&& properties) noexcept(std::is_nothrow_move_assignable_v<IchorProperties>) {
            _properties = std::forward<IchorProperties>(properties);
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
        template<class ServiceType>
        requires DerivedTemplated<ServiceType, Service>
        friend class LifecycleManager;
        template<class ServiceType>
        requires DerivedTemplated<ServiceType, Service>
        friend class DependencyLifecycleManager;
        template<class ServiceType>
        requires DerivedTemplated<ServiceType, Service>
        friend class LifecycleManager;
    };
}