#pragma once

#include <ichor/Dependency.h>
#include <ichor/Service.h>

namespace Ichor {
    struct DependencyRegister final {
        explicit DependencyRegister(DependencyManager *mng) noexcept;

        template<typename Interface, DerivedTemplated<Service> Impl>
        void registerDependency(Impl *svc, bool required, std::optional<Properties> props = {}) {
            if(_registrations.contains(typeNameHash<Interface>())) {
                throw std::runtime_error("Already registered interface");
            }

            _registrations.emplace(typeNameHash<Interface>(), std::make_tuple(
                    Dependency{typeNameHash<Interface>(), required},
                    Ichor::function<void(void*, IService*)>{[svc](void* dep, IService* isvc){ svc->addDependencyInstance(reinterpret_cast<Interface*>(dep), isvc); }, svc->getMemoryResource()},
                    Ichor::function<void(void*, IService*)>{[svc](void* dep, IService* isvc){ svc->removeDependencyInstance(reinterpret_cast<Interface*>(dep), isvc); }, svc->getMemoryResource()},
                    std::move(props)));
        }

        std::pmr::unordered_map<uint64_t, std::tuple<Dependency, Ichor::function<void(void*, IService*)>, Ichor::function<void(void*, IService*)>, std::optional<Properties>>> _registrations;
    };
}