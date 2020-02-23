#pragma once

#include <cstdint>
#include <atomic>
#include "sole.hpp"
#include "common.h"

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

    class Bundle {
    public:
        Bundle() noexcept;
        virtual ~Bundle();

    protected:
        [[nodiscard]] virtual bool start() = 0;
        [[nodiscard]] virtual bool stop() = 0;

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
        template<class Interface, class ComponentType>
        requires Derived<ComponentType, Bundle>
        friend class ComponentLifecycleManager;
    };
}