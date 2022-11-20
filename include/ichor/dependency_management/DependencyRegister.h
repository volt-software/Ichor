#pragma once

#include "Dependency.h"
#include "ichor/Service.h"
#include <optional>
#include <stdexcept>

namespace Ichor {
    struct DependencyRegister final {
        explicit DependencyRegister(DependencyManager *mng) noexcept {}

        template<typename Interface, DerivedTemplated<Service> Impl>
        void registerDependency(Impl *svc, bool required, std::optional<Properties> props = {}) {
            static_assert(!std::is_same_v<Interface, Impl>, "Impl and interface need to be separate classes");
            static_assert(!DerivedTemplated<Interface, Service>, "Interface needs to be a non-service class.");

            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (_registrations.contains(typeNameHash<Interface>())) [[unlikely]] {
                    throw std::runtime_error("Already registered interface");
                }
            }

            _registrations.emplace(typeNameHash<Interface>(), std::make_tuple(
                    Dependency{typeNameHash<Interface>(), required, 0},
                    std::function<void(void*, IService*)>{[svc](void* dep, IService* isvc){ svc->addDependencyInstance(reinterpret_cast<Interface*>(dep), isvc); }},
                    std::function<void(void*, IService*)>{[svc](void* dep, IService* isvc){ svc->removeDependencyInstance(reinterpret_cast<Interface*>(dep), isvc); }},
                    std::move(props)));
        }

        unordered_map<uint64_t, std::tuple<Dependency, std::function<void(void*, IService*)>, std::function<void(void*, IService*)>, std::optional<Properties>>> _registrations;
    };
}