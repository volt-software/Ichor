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
    };

    class Service : virtual public IService {
    public:
        Service() noexcept;
        Service(IchorProperties props, DependencyManager *mng = nullptr) noexcept;
        ~Service() noexcept override;

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
        [[nodiscard]] virtual bool start() = 0;
        [[nodiscard]] virtual bool stop() = 0;

        IchorProperties _properties;
    private:
        ///
        /// \return true if started
        [[nodiscard]] bool internal_start();
        ///
        /// \return true if stopped or already stopped
        [[nodiscard]] bool internal_stop();
        [[nodiscard]] ServiceState getState() const noexcept;
        void setProperties(IchorProperties&& properties) noexcept(std::is_nothrow_move_assignable_v<IchorProperties>);

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
        static std::atomic<uint64_t> _serviceIdCounter;
        DependencyManager *_manager{nullptr};

        friend class DependencyManager;
        template<class ServiceType>
        requires Derived<ServiceType, Service>
        friend class LifecycleManager;
        template<class ServiceType>
        requires Derived<ServiceType, Service>
        friend class DependencyLifecycleManager;
        template<class ServiceType>
        requires Derived<ServiceType, Service>
        friend class LifecycleManager;
    };
}