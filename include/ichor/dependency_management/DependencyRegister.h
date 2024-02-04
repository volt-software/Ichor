#pragma once

#include <ichor/dependency_management/Dependency.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <ichor/Concepts.h>
#include <tl/optional.h>

namespace Ichor {
    template <typename T>
    class AdvancedService;
    class IService;

    struct DependencyRegister final {

        template<typename Interface, DerivedTemplated<AdvancedService> Impl>
        void registerDependency(Impl *svc, DependencyFlags flags, tl::optional<Properties> props = {}) {
            static_assert(!std::is_same_v<Interface, Impl>, "Impl and interface need to be separate classes");
            static_assert(!DerivedTemplated<Interface, AdvancedService>, "Interface needs to be a non-service class.");
            // Some weird bug in MSVC
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
            static_assert(ImplementsDependencyInjection<Impl, Interface>, "Impl needs to implement the ImplementsDependencyInjection concept");
#endif

            _registrations.emplace(typeNameHash<Interface>(), std::make_tuple(
                    Dependency{typeNameHash<Interface>(), typeName<Interface>(), flags, 0},
                    std::function<void(NeverNull<void*>, IService&)>{[svc](NeverNull<void*> dep, IService& isvc){ svc->addDependencyInstance(*reinterpret_cast<Interface*>(dep.get()), isvc); }},
                    std::function<void(NeverNull<void*>, IService&)>{[svc](NeverNull<void*> dep, IService& isvc){ svc->removeDependencyInstance(*reinterpret_cast<Interface*>(dep.get()), isvc); }},
                    std::move(props)));
        }

        template<typename Interface, Derived<IService> Impl>
        void registerDependencyConstructor(Impl *svc) {
            static_assert(!std::is_same_v<Interface, Impl>, "Impl and interface need to be separate classes");
            static_assert(!DerivedTemplated<Interface, AdvancedService>, "Interface needs to be a non-service class.");

            _registrations.emplace(typeNameHash<Interface>(), std::make_tuple(
                    Dependency{typeNameHash<Interface>(), typeName<Interface>(), DependencyFlags::REQUIRED, 0},
                    std::function<void(NeverNull<void*>, IService&)>{[svc](NeverNull<void*> dep, IService& isvc){ svc->template addDependencyInstance<Interface>(reinterpret_cast<Interface*>(dep.get()), &isvc); }},
                    std::function<void(NeverNull<void*>, IService&)>{[svc](NeverNull<void*> dep, IService& isvc){ svc->template removeDependencyInstance<Interface>(reinterpret_cast<Interface*>(dep.get()), &isvc); }},
                    tl::optional<Properties>{}));
        }

        DependencyRegister() = default;
        DependencyRegister(const DependencyRegister&) = delete;
        DependencyRegister(DependencyRegister&&) noexcept = default;
        DependencyRegister& operator=(const DependencyRegister&) = delete;
        DependencyRegister& operator=(DependencyRegister&&) noexcept = default;

        std::unordered_multimap<uint64_t, std::tuple<Dependency, std::function<void(NeverNull<void*>, IService&)>, std::function<void(NeverNull<void*>, IService&)>, tl::optional<Properties>>> _registrations;

        [[nodiscard]]
        typename decltype(_registrations)::iterator begin();

        [[nodiscard]]
        typename decltype(_registrations)::const_iterator begin() const noexcept;

        [[nodiscard]]
        typename decltype(_registrations)::iterator end();

        [[nodiscard]]
        typename decltype(_registrations)::const_iterator end() const noexcept;

        [[nodiscard]]
        bool contains(const Dependency &dependency) const noexcept;

        [[nodiscard]]
        typename decltype(_registrations)::const_iterator find(const Dependency &dependency, bool satisfied) const noexcept;

        [[nodiscard]]
        typename decltype(_registrations)::iterator find(const Dependency &dependency, bool satisfied) noexcept;

        [[nodiscard]]
        typename decltype(_registrations)::const_iterator find(NameHashType hash, bool satisfied) const noexcept;

        [[nodiscard]]
        typename decltype(_registrations)::iterator find(NameHashType hash, bool satisfied) noexcept;

        [[nodiscard]]
        size_t size() const noexcept;

        [[nodiscard]]
        bool empty() const noexcept;

        [[nodiscard]]
        bool allSatisfied() const noexcept;
    };
}
