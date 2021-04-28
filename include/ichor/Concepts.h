#pragma once

#include <ichor/Common.h>
#include <ichor/Events.h>

namespace Ichor {

    struct DependencyRegister;
    class DependencyManager;
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

    template <class ImplT>
    concept RequestsDependencies = requires(ImplT impl, DependencyRegister &deps, Properties properties, DependencyManager *mng) {
        { ImplT(deps, properties, mng) };
    };

    template <class ImplT>
    concept RequestsProperties = requires(ImplT impl, Properties properties, DependencyManager *mng) {
        { ImplT(properties, mng) };
    };

    template <class ImplT, class Interface>
    concept ImplementsDependencyInjection = requires(ImplT impl, Interface *svc, IService *isvc) {
        { impl.addDependencyInstance(svc, isvc) } -> std::same_as<void>;
        { impl.removeDependencyInstance(svc, isvc) } -> std::same_as<void>;
    };

    template <class ImplT, class EventT>
    concept ImplementsEventCompletionHandlers = requires(ImplT impl, EventT const * const evt) {
        { impl.handleCompletion(evt) } -> std::same_as<void>;
        { impl.handleError(evt) } -> std::same_as<void>;
    };

    template <class ImplT, class EventT>
    concept ImplementsEventHandlers = requires(ImplT impl, EventT const * const evt) {
        { impl.handleEvent(evt) } -> std::same_as<Generator<bool>>;
    };

// TODO gcc 10.2 does not support the std::allocator_arg_t usage in coroutines/coroutine promises.
//  Decide whether to keep current setup with thread_local memory_resource or implement std::allocator_arg_t in gcc
//    template <class ImplT, class EventT>
//    concept ImplementsAllocatorEventHandlers = requires(ImplT impl, std::allocator_arg_t arg, Ichor::PolymorphicAllocator<bool> alloc, EventT const * const evt) {
//        { impl.handleEvent(arg, alloc, evt) } -> std::same_as<AllocatorGenerator<Ichor::PolymorphicAllocator<bool>, bool>>;
//    };

    template <class ImplT, class EventT>
    concept ImplementsEventInterceptors = requires(ImplT impl, EventT const * const evt, bool processed, uint32_t handlerAmount) {
        { impl.preInterceptEvent(evt) } -> std::same_as<bool>;
        { impl.postInterceptEvent(evt, processed) } -> std::same_as<void>;
    };

    template <class ImplT, class Interface>
    concept ImplementsTrackingHandlers = requires(ImplT impl, Interface *svc, DependencyRequestEvent const * const reqEvt, DependencyUndoRequestEvent const * const reqUndoEvt) {
        { impl.handleDependencyRequest(svc, reqEvt) } -> std::same_as<void>;
        { impl.handleDependencyUndoRequest(svc, reqUndoEvt) } -> std::same_as<void>;
    };
}