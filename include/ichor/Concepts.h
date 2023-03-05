#pragma once

#include <ichor/Common.h>
#include <ichor/events/InternalEvents.h>
#include <ichor/stl/NeverAlwaysNull.h>

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
    concept Derived = std::is_base_of<U, T>::value;

    template <class T, template <class> class U>
    concept DerivedTemplated = std::is_base_of<U<T>, T>::value;

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

    template <class ImplT, class Interface>
    concept ImplementsDependencyInjection = requires(ImplT impl, Interface* svc, IService* isvc) {
        { impl.addDependencyInstance(svc, isvc) } -> std::same_as<void>;
        { impl.removeDependencyInstance(svc, isvc) } -> std::same_as<void>;
    };

    template <class ImplT, class EventT>
    concept ImplementsEventCompletionHandlers = requires(ImplT impl, EventT const &evt) {
        { impl.handleCompletion(evt) } -> std::same_as<void>;
        { impl.handleError(evt) } -> std::same_as<void>;
    };

    template <class ImplT, class EventT>
    concept ImplementsEventHandlers = requires(ImplT impl, EventT const &evt) {
        { impl.handleEvent(evt) } -> std::same_as<AsyncGenerator<IchorBehaviour>>;
    };

    template <class ImplT, class EventT>
    concept ImplementsEventInterceptors = requires(ImplT impl, EventT const &evt, bool processed, uint32_t handlerAmount) {
        { impl.preInterceptEvent(evt) } -> std::same_as<bool>;
        { impl.postInterceptEvent(evt, processed) } -> std::same_as<void>;
    };

    template <class ImplT, class Interface>
    concept ImplementsTrackingHandlers = requires(ImplT impl, AlwaysNull<Interface*> svc, DependencyRequestEvent const &reqEvt, DependencyUndoRequestEvent const &reqUndoEvt) {
        { impl.handleDependencyRequest(svc, reqEvt) } -> std::same_as<void>;
        { impl.handleDependencyUndoRequest(svc, reqUndoEvt) } -> std::same_as<void>;
    };
}
