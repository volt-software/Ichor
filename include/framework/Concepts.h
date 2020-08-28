#pragma once

#include "Common.h"
#include "Events.h"

namespace Cppelix {

    class DependencyRegister;

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

    template <class T, class... U>
    concept ImplementsAll = DerivedTrait<T, U...>::value;

    template <class ImplT>
    concept RequestsDependencies = requires(ImplT impl, DependencyRegister &deps, CppelixProperties properties) {
        { ImplT(deps, properties) };
    };

    template <class ImplT, class Interface>
    concept ImplementsDependencyInjection = requires(ImplT impl, Interface *svc) {
        { impl.addDependencyInstance(svc) } -> std::same_as<void>;
        { impl.removeDependencyInstance(svc) } -> std::same_as<void>;
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

    template <class ImplT, class EventT>
    concept ImplementsEventInterceptors = requires(ImplT impl, EventT const * const evt, bool processed) {
        { impl.preInterceptEvent(evt) } -> std::same_as<bool>;
        { impl.postInterceptEvent(evt, processed) } -> std::same_as<bool>;
    };

    template <class ImplT, class Interface>
    concept ImplementsTrackingHandlers = requires(ImplT impl, Interface *svc, DependencyRequestEvent const * const reqEvt, DependencyUndoRequestEvent const * const reqUndoEvt) {
        { impl.handleDependencyRequest(svc, reqEvt) } -> std::same_as<void>;
        { impl.handleDependencyUndoRequest(svc, reqUndoEvt) } -> std::same_as<void>;
    };


    // TODO ServiceInterface on actual interface fails due to pure virtual functions or something
//    template <class T>
//    concept ServiceInterface = requires(T bundleInterface) {
//        { bundleInterface.version() } -> std::same_as<InterfaceVersion>;
//    };
}