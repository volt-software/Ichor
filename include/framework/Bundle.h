#pragma once

#include <cstdint>
#include <atomic>
#include "sole.hpp"

namespace Cppelix {
    class Framework;
    class DependencyManager;

    enum BundleState {
        UNINSTALLED,
        INSTALLED,
        RESOLVED,
        STARTING,
        STOPPING,
        ACTIVE,
        UNKNOWN
    };

    template <class T, class U>
    concept Derived = std::is_base_of<U, T>::value;

    class Bundle {
    public:
        Bundle();
        virtual ~Bundle();

        [[nodiscard]] virtual bool start() = 0;
        [[nodiscard]] virtual bool stop() = 0;
        [[nodiscard]] BundleState getState();

    private:
        [[nodiscard]] bool internal_start();
        [[nodiscard]] bool internal_stop();


        uint64_t _bundleId;
        sole::uuid _bundleGid;
        BundleState _bundleState;
        static std::atomic<uint64_t> _bundleIdCounter;

        friend class Framework;
        friend class DependencyManager;
        template<class ComponentType, class Impl>
        requires Derived<Impl, Bundle> friend class ComponentManager;
    };
}