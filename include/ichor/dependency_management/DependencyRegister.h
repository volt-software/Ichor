#pragma once

#include <ichor/dependency_management/Dependency.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <tl/optional.h>
#include <stdexcept>

namespace Ichor {
    struct DependencyRegister final {
        template<typename Interface, DerivedTemplated<AdvancedService> Impl>
        void registerDependency(Impl *svc, bool required, tl::optional<Properties> props = {}) {
            static_assert(!std::is_same_v<Interface, Impl>, "Impl and interface need to be separate classes");
            static_assert(!DerivedTemplated<Interface, AdvancedService>, "Interface needs to be a non-service class.");
            // Some weird bug in MSVC
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
            static_assert(ImplementsDependencyInjection<Impl, Interface>, "Impl needs to implement the ImplementsDependencyInjection concept");
#endif
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (_registrations.contains(typeNameHash<Interface>())) [[unlikely]] {
                    throw std::runtime_error("Already registered interface");
                }
            }

            _registrations.emplace(typeNameHash<Interface>(), std::make_tuple(
                    Dependency{typeNameHash<Interface>(), required, 0},
                    std::function<void(NeverNull<void*>, IService&)>{[svc](NeverNull<void*> dep, IService& isvc){ svc->addDependencyInstance(*reinterpret_cast<Interface*>(dep.get()), isvc); }},
                    std::function<void(NeverNull<void*>, IService&)>{[svc](NeverNull<void*> dep, IService& isvc){ svc->removeDependencyInstance(*reinterpret_cast<Interface*>(dep.get()), isvc); }},
                    std::move(props)));
        }

        template<typename Interface, Derived<IService> Impl>
        void registerDependencyConstructor(Impl *svc) {
            static_assert(!std::is_same_v<Interface, Impl>, "Impl and interface need to be separate classes");
            static_assert(!DerivedTemplated<Interface, AdvancedService>, "Interface needs to be a non-service class.");

            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (_registrations.contains(typeNameHash<Interface>())) [[unlikely]] {
                    throw std::runtime_error("Already registered interface");
                }
            }

            _registrations.emplace(typeNameHash<Interface>(), std::make_tuple(
                    Dependency{typeNameHash<Interface>(), true, 0},
                    std::function<void(NeverNull<void*>, IService&)>{[svc](NeverNull<void*> dep, IService& isvc){ svc->template addDependencyInstance<Interface>(reinterpret_cast<Interface*>(dep.get()), &isvc); }},
                    std::function<void(NeverNull<void*>, IService&)>{[svc](NeverNull<void*> dep, IService& isvc){ svc->template removeDependencyInstance<Interface>(reinterpret_cast<Interface*>(dep.get()), &isvc); }},
                    tl::optional<Properties>{}));
        }

        unordered_map<uint64_t, std::tuple<Dependency, std::function<void(NeverNull<void*>, IService&)>, std::function<void(NeverNull<void*>, IService&)>, tl::optional<Properties>>> _registrations;
    };
}
