#pragma once

#include <cstdint>
#include <atomic>
#include "sole.hpp"
#include "Common.h"
#include "Concepts.h"

namespace Cppelix {
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

        [[nodiscard]] virtual uint64_t getServiceId() const = 0;

        [[nodiscard]] virtual DependencyManager* getManager() = 0;

        [[nodiscard]] virtual CppelixProperties* getProperties() = 0;
    };

    class Service : virtual public IService {
    public:
        Service() noexcept;
        ~Service() override;

        void injectDependencyManager(DependencyManager *mng) {
            _manager = mng;
        }

        [[nodiscard]] uint64_t getServiceId() const final {
            return _serviceId;
        }

        [[nodiscard]] DependencyManager* getManager() final {
            return _manager;
        }

        [[nodiscard]] CppelixProperties* getProperties() final {
            return &_properties;
        }

    protected:
        [[nodiscard]] virtual bool start() = 0;
        [[nodiscard]] virtual bool stop() = 0;

        CppelixProperties _properties;
    private:
        ///
        /// \return true if started
        [[nodiscard]] bool internal_start();
        ///
        /// \return true if stopped or already stopped
        [[nodiscard]] bool internal_stop();
        [[nodiscard]] ServiceState getState() const noexcept;
        void setProperties(CppelixProperties&& properties);


        uint64_t _serviceId;
        sole::uuid _serviceGid;
        ServiceState _serviceState;
        static std::atomic<uint64_t> _serviceIdCounter;
        DependencyManager *_manager;

        friend class DependencyManager;
        template<class ServiceType, typename... Dependencies>
        requires Derived<ServiceType, Service>
        friend class LifecycleManager;
        template<class ServiceType>
        requires Derived<ServiceType, Service>
        friend class ServiceLifecycleManager;
    };
}