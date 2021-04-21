#pragma once

#include <ichor/Dependency.h>
#include <ichor/Service.h>

namespace Ichor {
    struct DependencyRegister final {
        explicit DependencyRegister(DependencyManager *mng) noexcept;

        template<Derived<IService> Interface, DerivedTemplated<Service> Impl>
        void registerDependency(Impl *svc, bool required, std::optional<IchorProperties> props = {}) {
            if(_registrations.contains(typeNameHash<Interface>())) {
                throw std::runtime_error("Already registered interface");
            }

            _registrations.emplace(typeNameHash<Interface>(), std::make_tuple(
                    Dependency{typeNameHash<Interface>(), required},
                    Ichor::function<void(IService*)>{[svc](IService* dep){ svc->addDependencyInstance(reinterpret_cast<Interface*>(dep)); }, svc->getMemoryResource()},
                    Ichor::function<void(IService*)>{[svc](IService* dep){ svc->removeDependencyInstance(reinterpret_cast<Interface*>(dep)); }, svc->getMemoryResource()},
                    std::move(props)));
        }

        std::pmr::unordered_map<uint64_t, std::tuple<Dependency, Ichor::function<void(IService*)>, Ichor::function<void(IService*)>, std::optional<IchorProperties>>> _registrations;
    };
}