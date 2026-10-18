#pragma once

#include <ichor/Common.h>
#include <ichor/events/InternalEvents.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <ichor/ScopedServiceProxy.h>
#include <concepts>
#include <utility>

#include "Concepts.h"

namespace Ichor {

    struct DependencyRegister;
    class IService;

    template <class T, class U, class... Remainder>
    struct DerivedTrait : std::integral_constant<bool, DerivedTrait<T, Remainder...>::value && std::is_base_of_v<U, T>> {};

    template <class T, class U>
    struct DerivedTrait<T, U> : std::integral_constant<bool, std::is_base_of_v<U, T>> {};

    template <class T, class U, class... Remainder>
    struct ListContainsInterface : std::integral_constant<bool, ListContainsInterface<T, Remainder...>::value || std::is_base_of_v<U, T>> {};

    template <class T, class U>
    struct ListContainsInterface<T, U> : std::integral_constant<bool, std::is_base_of_v<U, T>> {};

    template <class T, class U>
    concept Derived = std::is_base_of_v<U, T>;

    template <class T, class U, class V>
    concept DerivedEither = std::is_base_of_v<U, T> || std::is_base_of_v<V, T>;

    template <class T, class U, class V, class X>
    concept DerivedAny = std::is_base_of_v<U, T> || std::is_base_of_v<V, T> || std::is_base_of_v<X, T>;

    template <class T, template <class> class U>
    concept DerivedTemplated = std::is_base_of_v<U<T>, T>;

    template <class T, class... U>
    concept ImplementsAll = sizeof...(U) == 0 || DerivedTrait<T, U...>::value;

    template <template<class> class SERVICE_T, class T, class... U>
    concept ConstructorInjectionImplementsAll = sizeof...(U) == 0 || DerivedTrait<T, U...>::value;

    // check if `ConstructorInjector` is usable in a constexpr setting
    template<bool> using ConstructorInjectorHelper = void;
    template <class ImplT>
    concept IsConstructorInjector = requires(ImplT impl) {
        typename ConstructorInjectorHelper<ImplT::ConstructorInjector>;
    };

    template <class ImplT>
    concept RequestsDependencies = requires(ImplT impl, DependencyRegister &deps, Properties properties) {
        { ImplT(deps, properties) };
    };

    template <class ImplT>
    concept RequestsProperties = requires(ImplT impl, Properties properties) {
        { ImplT(properties) };
    };

    // The checks for the dependency injection, event handler, event interceptor and dependency tracker functions are not concepts but are
    // written inline in DependencyRegister and DependencyManager: clang 23+ checks access in a concept from the concept's context,
    // so services that keep these functions private and befriend DependencyRegister/DependencyManager would not satisfy them.
}
