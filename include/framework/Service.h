#pragma once

#include <cstdint>
#include <atomic>
#include "sole.hpp"
#include "Common.h"

namespace Cppelix {
    class Framework;
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
        virtual ~IService() {}

        virtual uint64_t get_service_id() const = 0;
    };

    class Service : virtual public IService {
    public:
        Service() noexcept;
        ~Service() override;



        void injectDependencyManager(DependencyManager *mng) {
            _manager = mng;
        }

        uint64_t get_service_id() const final {
            return _serviceId;
        }

    protected:
        [[nodiscard]] virtual bool start() = 0;
        [[nodiscard]] virtual bool stop() = 0;

        DependencyManager *_manager;
    private:
        [[nodiscard]] bool internal_start();
        [[nodiscard]] bool internal_stop();
        [[nodiscard]] ServiceState getState() const noexcept;


        uint64_t _serviceId;
        sole::uuid _serviceGid;
        ServiceState _serviceState;
        static std::atomic<uint64_t> _serviceIdCounter;

        friend class Framework;
        friend class DependencyManager;
        template<class Interface, class ComponentType, typename... Dependencies>
        requires Derived<ComponentType, Service>
        friend class DependencyComponentLifecycleManager;
        template<class Interface, class ComponentType>
        requires Derived<ComponentType, Service>
        friend class ComponentLifecycleManager;
    };
}