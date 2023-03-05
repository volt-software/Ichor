#pragma once

#include <ichor/dependency_management/Dependency.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <optional>
#include <stdexcept>

namespace Ichor {
    struct DependencyRegister final {
        template<typename Interface, DerivedTemplated<AdvancedService> Impl>
        void registerDependency(Impl *svc, bool required, std::optional<Properties> props = {}) {
            static_assert(!std::is_same_v<Interface, Impl>, "Impl and interface need to be separate classes");
            static_assert(!DerivedTemplated<Interface, AdvancedService>, "Interface needs to be a non-service class.");
            static_assert(ImplementsDependencyInjection<Impl, Interface>, "Impl needs to implement the ImplementsDependencyInjection concept");

            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (_registrations.contains(typeNameHash<Interface>())) [[unlikely]] {
                    throw std::runtime_error("Already registered interface");
                }
            }

            _registrations.emplace(typeNameHash<Interface>(), std::make_tuple(
                    Dependency{typeNameHash<Interface>(), required, 0},
                    std::function<void(NeverNull<void*>, NeverNull<IService*>)>{[svc](NeverNull<void*> dep, NeverNull<IService*> isvc){ svc->addDependencyInstance(reinterpret_cast<Interface*>(dep.get()), isvc); }},
                    std::function<void(NeverNull<void*>, NeverNull<IService*>)>{[svc](NeverNull<void*> dep, NeverNull<IService*> isvc){ svc->removeDependencyInstance(reinterpret_cast<Interface*>(dep.get()), isvc); }},
                    std::move(props)));
        }

        template<typename Interface, Derived<IService> Impl>
        void registerDependencyConstructor(Impl *svc) {
            static_assert(!std::is_same_v<Interface, Impl>, "Impl and interface need to be separate classes");
            static_assert(!DerivedTemplated<Interface, AdvancedService>, "Interface needs to be a non-service class.");
            static_assert(ImplementsDependencyInjection<Impl, Interface>, "Impl needs to implement the ImplementsDependencyInjection concept");

            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (_registrations.contains(typeNameHash<Interface>())) [[unlikely]] {
                    throw std::runtime_error("Already registered interface");
                }
            }

            _registrations.emplace(typeNameHash<Interface>(), std::make_tuple(
                    Dependency{typeNameHash<Interface>(), true, 0},
                    std::function<void(NeverNull<void*>, NeverNull<IService*>)>{[svc](NeverNull<void*> dep, NeverNull<IService*> isvc){ svc->template addDependencyInstance<Interface>(reinterpret_cast<Interface*>(dep.get()), isvc); }},
                    std::function<void(NeverNull<void*>, NeverNull<IService*>)>{[svc](NeverNull<void*> dep, NeverNull<IService*> isvc){ svc->template removeDependencyInstance<Interface>(reinterpret_cast<Interface*>(dep.get()), isvc); }},
                    std::optional<Properties>{}));
        }

        unordered_map<uint64_t, std::tuple<Dependency, std::function<void(NeverNull<void*>, NeverNull<IService*>)>, std::function<void(NeverNull<void*>, NeverNull<IService*>)>, std::optional<Properties>>> _registrations;
    };
}
