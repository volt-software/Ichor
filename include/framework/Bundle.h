#pragma once

#include <cstdint>
#include <atomic>
#include "sole.hpp"
#include "Common.h"

namespace Cppelix {
    class Framework;
    class DependencyManager;

    enum class BundleState {
        UNINSTALLED,
        INSTALLED,
        RESOLVED,
        STARTING,
        STOPPING,
        ACTIVE,
        UNKNOWN
    };

    class IBundle {
    public:
        virtual ~IBundle() {}

        virtual uint64_t get_bundle_id() const = 0;
    };

    class Bundle : virtual public IBundle {
    public:
        Bundle() noexcept;
        ~Bundle() override;



        void injectDependencyManager(DependencyManager *mng) {
            _manager = mng;
        }

        uint64_t get_bundle_id() const final {
            return _bundleId;
        }

    protected:
        [[nodiscard]] virtual bool start() = 0;
        [[nodiscard]] virtual bool stop() = 0;

        DependencyManager *_manager;
    private:
        [[nodiscard]] bool internal_start();
        [[nodiscard]] bool internal_stop();
        [[nodiscard]] BundleState getState() const noexcept;


        uint64_t _bundleId;
        sole::uuid _bundleGid;
        BundleState _bundleState;
        static std::atomic<uint64_t> _bundleIdCounter;

        friend class Framework;
        friend class DependencyManager;
        template<class Interface, class ComponentType, typename... Dependencies>
        requires Derived<ComponentType, Bundle>
        friend class DependencyComponentLifecycleManager;
        template<class Interface, class ComponentType>
        requires Derived<ComponentType, Bundle>
        friend class ComponentLifecycleManager;
    };
}