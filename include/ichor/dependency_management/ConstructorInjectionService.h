#pragma once

#include <ichor/Concepts.h>
#include <ichor/coroutines/Task.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/dependency_management/IService.h>
#include <ichor/ScopedServiceProxy.h>
#include <utility>
#include <variant>

namespace Ichor {

    // Taken from https://gist.githubusercontent.com/deni64k/c5728d0596f8f1640318b357701f43e6/raw/87ea05a8f7b3f6add5b3775fecf089e0aa421492/reflection.hxx
    // See also https://stackoverflow.com/a/54493136
    namespace refl {
#if !defined(__clang__) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-template-friend"
#endif
        template <typename T>
        struct unwrap_dependency;

        template <typename U>
        struct unwrap_dependency<Ichor::ScopedServiceProxy<U>> {
            using type = U*;
        };

        template <>
        struct unwrap_dependency<IService*> {
            using type = IService*;
        };

        // template <>
        // struct unwrap_dependency<IService> {
        //     using type = IService;
        // };

        template <>
        struct unwrap_dependency<IEventQueue*> {
            using type = IEventQueue*;
        };

        // template <>
        // struct unwrap_dependency<IEventQueue> {
        //     using type = IEventQueue;
        // };

        template <>
        struct unwrap_dependency<DependencyManager*> {
            using type = DependencyManager*;
        };

        // template <>
        // struct unwrap_dependency<DependencyManager> {
        //     using type = DependencyManager;
        // };

        template <typename T>
        using unwrap_dependency_t = typename unwrap_dependency<T>::type;

        template <typename T>
        struct wrap_only_if_not_known_types {
            using type = ScopedServiceProxy<std::remove_pointer_t<T>>;
        };

        template <>
        struct wrap_only_if_not_known_types<IService*> {
            using type = IService*;
        };

        template <>
        struct wrap_only_if_not_known_types<IService> {
            using type = IService*;
        };

        template <>
        struct wrap_only_if_not_known_types<IEventQueue*> {
            using type = IEventQueue*;
        };

        template <>
        struct wrap_only_if_not_known_types<IEventQueue> {
            using type = IEventQueue*;
        };

        template <>
        struct wrap_only_if_not_known_types<DependencyManager*> {
            using type = DependencyManager*;
        };

        template <>
        struct wrap_only_if_not_known_types<DependencyManager> {
            using type = DependencyManager*;
        };

        template <typename T>
        using wrap_only_if_not_known_types_t = typename wrap_only_if_not_known_types<T>::type;

        template <typename T>
        struct rewrap_dependencies;

        template <typename... Ts>
        struct rewrap_dependencies<std::variant<Ts...>> {
            using type = std::variant<wrap_only_if_not_known_types_t<Ts>...>;
        };

        template <typename T>
        using rewrap_dependencies_t = typename rewrap_dependencies<T>::type;

        // Based on
        // * http://alexpolt.github.io/type-loophole.html
        //   https://github.com/alexpolt/luple/blob/master/type-loophole.h
        //   by Alexandr Poltavsky, http://alexpolt.github.io
        // * https://www.youtube.com/watch?v=UlNUNxLtBI0
        //   Better C++14 reflections - Antony Polukhin - Meeting C++ 2018

        // tag<T, N> generates friend declarations and helps with overload resolution.
        // There are two types: one with the auto return type, which is the way we read types later.
        // The second one is used in the detection of instantiations without which we'd get multiple
        // definitions.
        template <typename T, int N>
        struct tag {
            friend auto loophole(tag<T, N>);
            constexpr friend int cloophole(tag<T, N>);
        };

        // The definitions of friend functions.
        template <typename T, typename U, int N, bool B,
                typename = typename std::enable_if_t<
                        !std::is_same_v<
                                std::remove_cv_t<std::remove_reference_t<T>>,
                                std::remove_cv_t<std::remove_reference_t<U>>>>>
        struct fn_def {
            friend auto loophole(tag<T, N>) { return U{}; }
            constexpr friend int cloophole(tag<T, N>) { return 0; }
        };

        // This specialization is to avoid multiple definition errors.
        template <typename T, typename U, int N> struct fn_def<T, U, N, true> {};

        // This has a templated conversion operator which in turn triggers instantiations.
        // Important point, using sizeof seems to be more reliable. Also default template
        // arguments are "cached" (I think). To fix that I provide a U template parameter to
        // the ins functions which do the detection using constexpr friend functions and SFINAE.
        template <typename T, int N>
        struct c_op {
            template <typename U, int M>
            static auto ins(...) -> int;
            template <typename U, int M, int = cloophole(tag<T, M>{})>
            static auto ins(int) -> char;

            template <typename U, int = sizeof(fn_def<T, U, N, sizeof(ins<U, N>(0)) == sizeof(char)>)>
            operator U();
        };

        // Here we detect the data type field number. The byproduct is instantiations.
        // Uses list initialization. Won't work for types with user-provided constructors.
        // In C++17 there is std::is_aggregate which can be added later.
        template <typename T, int... Ns>
        constexpr int fields_number(...) { return sizeof...(Ns) - 1; }

        template <typename T, int... Ns>
        constexpr auto fields_number(int) -> decltype(T{c_op<T, Ns>{}...}, 0) {
            return fields_number<T, Ns..., sizeof...(Ns)>(0);
        }

        // Here is a version of fields_number to handle user-provided ctor.
        // NOTE: It finds the first ctor having the shortest unambigious set
        //       of parameters.
        template <typename T, int... Ns>
        constexpr auto fields_number_ctor(int) -> decltype(T(c_op<T, Ns>{}...), 0) {
            return sizeof...(Ns);
        }

        template <typename T, int... Ns>
        constexpr int fields_number_ctor(...) {
            static_assert(sizeof...(Ns) < 64,
                  "Constructor injection supports at most 64 dependencies");
            return fields_number_ctor<T, Ns..., sizeof...(Ns)>(0);
        }

        // This is a helper to turn a ctor into a variant type.
        // Usage is: refl::as_variant<data_t>
        template <typename T, typename U> struct loophole_variant;

        template <typename T, int... Ns>
        struct loophole_variant<T, std::integer_sequence<int, Ns...>> {
            using type = std::variant<refl::unwrap_dependency_t<decltype(loophole(tag<T, Ns>{}))>...>;
        };

        template <typename T>
        using as_variant =
                typename loophole_variant<T, std::make_integer_sequence<int, fields_number_ctor<T>(0)>>::type;

#if !defined(__clang__) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    }  // namespace refl

    namespace Detail {
        extern std::atomic<uint64_t> _serviceIdCounter;

        template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires DerivedTemplated<ServiceType, AdvancedService> || IsConstructorInjector<ServiceType>
#endif
        class LifecycleManager;
        template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<ServiceType, IService>
#endif
        class DependencyLifecycleManager;
    }

    template <class ImplT>
    concept HasConstructorInjectionDependencies = refl::fields_number_ctor<ImplT>(0) > 0;

    template <class ImplT>
    concept DoesNotHaveConstructorInjectionDependencies = refl::fields_number_ctor<ImplT>(0) == 0;

    template <typename T>
    class ConstructorInjectionService;

    template <HasConstructorInjectionDependencies T>
    class ConstructorInjectionService<T> final : public IService {
    public:
        ConstructorInjectionService(DependencyRegister &reg, Properties props) noexcept : IService(), _properties(std::move(props)), _serviceId(ServiceIdType{Detail::_serviceIdCounter.fetch_add(1, std::memory_order_relaxed)}), _servicePriority(INTERNAL_EVENT_PRIORITY), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED) {
            registerDependenciesSpecialSauce(reg, tl::optional<refl::as_variant<T>>());
        }

        ~ConstructorInjectionService() noexcept final {
            _serviceState = ServiceState::UNINSTALLED;
        }

        ConstructorInjectionService(const ConstructorInjectionService&) = default;
        ConstructorInjectionService(ConstructorInjectionService&&) noexcept = default;
        ConstructorInjectionService& operator=(const ConstructorInjectionService&) = default;
        ConstructorInjectionService& operator=(ConstructorInjectionService&&) noexcept = default;

        /// Process-local unique service id
        /// \return id
        [[nodiscard]] ICHOR_PURE_FUNC_ATTR ServiceIdType getServiceId() const noexcept final {
            return _serviceId;
        }

        /// Global unique service id
        /// \return gid
        [[nodiscard]] ICHOR_PURE_FUNC_ATTR sole::uuid getServiceGid() const noexcept final {
            return _serviceGid;
        }

        /// Name of the user-specified service (e.g. CoutFrameworkLogger)
        /// \return
        [[nodiscard]] ICHOR_PURE_FUNC_ATTR std::string_view getServiceName() const noexcept final {
            return typeName<T>();
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _serviceState;
        }

        [[nodiscard]] uint64_t getServicePriority() const noexcept final {
            return _servicePriority;
        }

        void setServicePriority(uint64_t priority) noexcept final {
            _servicePriority = priority;
        }

        [[nodiscard]] bool isStarted() const noexcept final {
            return _serviceState == ServiceState::ACTIVE;
        }

        [[nodiscard]] Properties& getProperties() noexcept final {
            return _properties;
        }

        [[nodiscard]] const Properties& getProperties() const noexcept final {
            return _properties;
        }

        // checked by IsConstructorInjector concept, to help with function overload resolution
        static constexpr bool ConstructorInjector = true;

    protected:
        Properties _properties;
    private:
#ifdef ICHOR_ENABLE_INTERNAL_DEBUGGING
        template <typename CO_ARG>
        void logSingleService(std::string &out) {
            if constexpr(std::is_same_v<CO_ARG, IService*>) {
                fmt::format_to(std::back_inserter(out), " IService:{}:{}", typeNameHash<IService>(), typeNameHash<IService*>());
            } else if constexpr(std::is_same_v<CO_ARG, DependencyManager*>) {
                fmt::format_to(std::back_inserter(out), " DependencyManager:{}:{}", typeNameHash<DependencyManager>(), typeNameHash<DependencyManager*>());
            } else if constexpr(std::is_same_v<CO_ARG, IEventQueue*>) {
                fmt::format_to(std::back_inserter(out), " IEventQueue:{}:{}", typeNameHash<IEventQueue>(), typeNameHash<IEventQueue*>());
            } else {
                fmt::format_to(std::back_inserter(out), " {}:{}:{}:{}",
                    typeName<refl::wrap_only_if_not_known_types_t<CO_ARG>>(),
                    typeNameHash<refl::wrap_only_if_not_known_types_t<CO_ARG>>(),
                    std::get<refl::wrap_only_if_not_known_types_t<CO_ARG>>(_deps.at(typeNameHash<refl::wrap_only_if_not_known_types_t<CO_ARG>>()))._serviceId,
                    static_cast<void*>(std::get<refl::wrap_only_if_not_known_types_t<CO_ARG>>(_deps.at(typeNameHash<refl::wrap_only_if_not_known_types_t<CO_ARG>>()))._service));
            }
        }
        template <typename... CO_ARGS>
        void logEmplaceService() {
            std::string out;
            fmt::format_to(std::back_inserter(out), "Emplacing service {} {}", typeName<T>(), typeName<refl::rewrap_dependencies_t<refl::as_variant<T>>>());
            (logSingleService<CO_ARGS>(out), ...);
            for(auto const &[hash, dep] : _deps) {
                fmt::format_to(std::back_inserter(out), "\nrequested dep {}", hash);
            }
            INTERNAL_DEBUG("{}", out.data());
        }
#endif

        template <typename... CO_ARGS>
        void registerDependenciesSpecialSauce(DependencyRegister &reg, tl::optional<std::variant<CO_ARGS...>> = {}) {
            (reg.registerDependencyConstructor<std::remove_pointer_t<CO_ARGS>>(this), ...);
        }

        template <typename... CO_ARGS>
        void createServiceSpecialSauce(tl::optional<std::variant<CO_ARGS...>> = {}) {

#if ICHOR_EXCEPTIONS_ENABLED
            try {
#endif
#ifdef ICHOR_ENABLE_INTERNAL_DEBUGGING
                logEmplaceService<CO_ARGS...>();
#endif
                new(buf) T((std::get<refl::wrap_only_if_not_known_types_t<CO_ARGS>>(_deps.at(typeNameHash<refl::wrap_only_if_not_known_types_t<CO_ARGS>>())))...);
#if ICHOR_EXCEPTIONS_ENABLED
            } catch (std::exception const &e) {
                fmt::print("Std exception while starting svc {}:{} : \"{}\".\n\nLikely user error, but printing extra information anyway: Stored dependencies:\n", getServiceId(), getServiceName(), e.what());
                for(auto &[key, _] : _deps) {
                    fmt::print("{} ", key);
                }
                fmt::print("\n");

                fmt::print("Requested deps:\n");
                (fmt::print("{}:{} ", typeNameHash<CO_ARGS>(), typeName<CO_ARGS>()), ...);
                fmt::print("\n");
                std::terminate();
            } catch (...) {
                fmt::print("Unknown exception while starting svc {}:{}.\n\nLikely user error, but printing extra information anyway: Stored dependencies:\n", getServiceId(), getServiceName());
                for(auto &[key, _] : _deps) {
                    fmt::print("{} ", key);
                }
                fmt::print("\n");

                fmt::print("Requested deps:\n");
                (fmt::print("{}:{} ", typeNameHash<CO_ARGS>(), typeName<CO_ARGS>()), ...);
                fmt::print("\n");
                std::terminate();
            }
#endif
        }

        ///
        /// \return
        [[nodiscard]] Task<StartBehaviour> internal_start(DependencyRegister const *_dependencies) {
            if(_serviceState != ServiceState::INSTALLED || (_dependencies != nullptr && !_dependencies->allSatisfied())) {
                INTERNAL_DEBUG("internal_start service {}:{} state {} dependencies {} {}", getServiceId(), typeName<T>(), getState(), _dependencies->size(), _dependencies->allSatisfied());
                co_return {};
            }

            INTERNAL_DEBUG("internal_start service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::STARTING);
            _serviceState = ServiceState::STARTING;
            createServiceSpecialSauce(tl::optional<refl::as_variant<T>>());

            INTERNAL_DEBUG("internal_start service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::INJECTING);
            _serviceState = ServiceState::INJECTING;

            co_return {};
        }
        ///
        /// \return
        [[nodiscard]] Task<StartBehaviour> internal_stop() {
            if(_serviceState == ServiceState::INSTALLED) [[unlikely]] {
                co_return {};
            }

            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if(_serviceState != ServiceState::UNINJECTING) [[unlikely]] {
                    std::terminate();
                }
            }

            INTERNAL_DEBUG("internal_stop service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::STOPPING);
            _serviceState = ServiceState::STOPPING;
            reinterpret_cast<T*>(buf)->~T();

            INTERNAL_DEBUG("internal_stop service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::INSTALLED);
            _serviceState = ServiceState::INSTALLED;

            co_return {};
        }

        [[nodiscard]] bool internalSetInjected() {
            if(_serviceState != ServiceState::INJECTING) {
                return false;
            }

            INTERNAL_DEBUG("internalSetInjected service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::ACTIVE);
            _serviceState = ServiceState::ACTIVE;
            return true;
        }

        [[nodiscard]] bool internalSetUninjected() {
            if(_serviceState != ServiceState::ACTIVE) {
                return false;
            }

            INTERNAL_DEBUG("internalSetUninjected service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::UNINJECTING);
            _serviceState = ServiceState::UNINJECTING;
            return true;
        }

        [[nodiscard]] ServiceState getState() const noexcept {
            return _serviceState;
        }

        void setProperties(Properties&& properties) noexcept(std::is_nothrow_move_assignable_v<Properties>) {
            _properties = std::move(properties);
        }

        template <typename depT>
        void addDependencyInstance(Ichor::ScopedServiceProxy<depT> dep, [[maybe_unused]] v1::NeverNull<IService*> isvc) {
            INTERNAL_DEBUG("Adding service {}:{}:{} {}", isvc->getServiceId(), typeName<depT>(), typeNameHash<Ichor::ScopedServiceProxy<depT>>(), typeName<refl::rewrap_dependencies_t<refl::as_variant<T>>>());
            _deps.emplace(std::pair<uint64_t, refl::rewrap_dependencies_t<refl::as_variant<T>>>{typeNameHash<Ichor::ScopedServiceProxy<depT>>(), dep});
        }

        void addDependencyInstance(DependencyManager *dep, [[maybe_unused]] v1::NeverNull<IService*> isvc) {
            INTERNAL_DEBUG("Adding service {}:DependencyManager", isvc->getServiceId());
            _deps.emplace(std::pair<uint64_t, refl::rewrap_dependencies_t<refl::as_variant<T>>>{typeNameHash<DependencyManager*>(), dep});
        }

        void addDependencyInstance(IService *dep, [[maybe_unused]] v1::NeverNull<IService*> isvc) {
            INTERNAL_DEBUG("Adding service {}:IService", isvc->getServiceId());
            _deps.emplace(std::pair<uint64_t, refl::rewrap_dependencies_t<refl::as_variant<T>>>{typeNameHash<IService*>(), dep});
        }

        void addDependencyInstance(IEventQueue *dep, [[maybe_unused]] v1::NeverNull<IService*> isvc) {
            INTERNAL_DEBUG("Adding service {}:IEventQueue", isvc->getServiceId());
            _deps.emplace(std::pair<uint64_t, refl::rewrap_dependencies_t<refl::as_variant<T>>>{typeNameHash<IEventQueue*>(), dep});
        }

        template <typename depT>
        void removeDependencyInstance(Ichor::ScopedServiceProxy<depT>, [[maybe_unused]] v1::NeverNull<IService*> isvc) {
            INTERNAL_DEBUG("Removing service {}:{}", isvc->getServiceId(), typeName<Ichor::ScopedServiceProxy<depT>>());
            // if(_serviceState == ServiceState::UNINJECTING) {
            //     auto gen = [this]() -> AsyncGenerator<StartBehaviour> {
            //         co_return co_await internal_stop();
            //     }();
            //     auto it = gen.begin();
            //     if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
            //         if (!it.get_finished()) {
            //             INTERNAL_DEBUG("service {}:{} state {} failed to run internal_stop to completion", getServiceId(), typeName<T>(), getState());
            //             std::terminate();
            //         }
            //     }
            // }
            _deps.erase(typeNameHash<Ichor::ScopedServiceProxy<depT>>());
        }

        void removeDependencyInstance(DependencyManager *, [[maybe_unused]] v1::NeverNull<IService*> isvc) {
            INTERNAL_DEBUG("Removing service {}:DependencyManager", isvc->getServiceId());
            // if(_serviceState == ServiceState::UNINJECTING) {
            //     auto gen = [this]() -> AsyncGenerator<StartBehaviour> {
            //         co_return co_await internal_stop();
            //     }();
            //     auto it = gen.begin();
            //     if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
            //         if (!it.get_finished()) {
            //             INTERNAL_DEBUG("service {}:{} state {} failed to run internal_stop to completion", getServiceId(), typeName<T>(), getState());
            //             std::terminate();
            //         }
            //     }
            // }
            _deps.erase(typeNameHash<DependencyManager*>());
        }

        void removeDependencyInstance(IService *, [[maybe_unused]] v1::NeverNull<IService*> isvc) {
            INTERNAL_DEBUG("Removing service {}:IService", isvc->getServiceId());
            // if(_serviceState == ServiceState::UNINJECTING) {
            //     auto gen = [this]() -> AsyncGenerator<StartBehaviour> {
            //         co_return co_await internal_stop();
            //     }();
            //     auto it = gen.begin();
            //     if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
            //         if (!it.get_finished()) {
            //             INTERNAL_DEBUG("service {}:{} state {} failed to run internal_stop to completion", getServiceId(), typeName<T>(), getState());
            //             std::terminate();
            //         }
            //     }
            // }
            _deps.erase(typeNameHash<IService*>());
        }

        void removeDependencyInstance(IEventQueue *, [[maybe_unused]] v1::NeverNull<IService*> isvc) {
            INTERNAL_DEBUG("Removing service {}:IEventQueue", isvc->getServiceId());
            // if(_serviceState == ServiceState::UNINJECTING) {
            //     auto gen = [this]() -> AsyncGenerator<StartBehaviour> {
            //         co_return co_await internal_stop();
            //     }();
            //     auto it = gen.begin();
            //     if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
            //         if (!it.get_finished()) {
            //             INTERNAL_DEBUG("service {}:{} state {} failed to run internal_stop to completion", getServiceId(), typeName<T>(), getState());
            //             std::terminate();
            //         }
            //     }
            // }
            _deps.erase(typeNameHash<IEventQueue*>());
        }

        [[nodiscard]] T* getImplementation() noexcept {
            if(_serviceState < ServiceState::INJECTING) {
                std::terminate();
            }
            return reinterpret_cast<T*>(buf);
        }


        ServiceIdType const _serviceId;
        uint64_t _servicePriority;
        sole::uuid const _serviceGid;
        ServiceState _serviceState;
        unordered_map<uint64_t, refl::rewrap_dependencies_t<refl::as_variant<T>>> _deps{};
        alignas(T) std::byte buf[sizeof(T)];

        friend struct DependencyRegister;

        template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<ServiceType, IService>
#endif
        friend class Detail::DependencyLifecycleManager;

    };

    template <DoesNotHaveConstructorInjectionDependencies T>
    class ConstructorInjectionService<T> final : public IService {
    public:
        ConstructorInjectionService(Properties props) noexcept : IService(), _properties(std::move(props)), _serviceId(ServiceIdType{Detail::_serviceIdCounter.fetch_add(1, std::memory_order_relaxed)}), _servicePriority(INTERNAL_EVENT_PRIORITY), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED) {
        }

        ~ConstructorInjectionService() noexcept final {
            _serviceState = ServiceState::UNINSTALLED;
        }

        ConstructorInjectionService(const ConstructorInjectionService&) = default;
        ConstructorInjectionService(ConstructorInjectionService&&) noexcept = default;
        ConstructorInjectionService& operator=(const ConstructorInjectionService&) = default;
        ConstructorInjectionService& operator=(ConstructorInjectionService&&) noexcept = default;

        /// Process-local unique service id
        /// \return id
        [[nodiscard]] ICHOR_PURE_FUNC_ATTR ServiceIdType getServiceId() const noexcept final {
            return _serviceId;
        }

        /// Global unique service id
        /// \return gid
        [[nodiscard]] ICHOR_PURE_FUNC_ATTR sole::uuid getServiceGid() const noexcept final {
            return _serviceGid;
        }

        /// Name of the user-specified service (e.g. CoutFrameworkLogger)
        /// \return
        [[nodiscard]] ICHOR_PURE_FUNC_ATTR std::string_view getServiceName() const noexcept final {
            return typeName<T>();
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _serviceState;
        }

        [[nodiscard]] uint64_t getServicePriority() const noexcept final {
            return _servicePriority;
        }

        void setServicePriority(uint64_t priority) noexcept final {
            _servicePriority = priority;
        }

        [[nodiscard]] bool isStarted() const noexcept final {
            return _serviceState == ServiceState::ACTIVE;
        }

        [[nodiscard]] Properties& getProperties() noexcept final {
            return _properties;
        }

        [[nodiscard]] const Properties& getProperties() const noexcept final {
            return _properties;
        }

        [[nodiscard]] T* getImplementation() noexcept {
            if(_serviceState < ServiceState::INJECTING) {
                std::terminate();
            }
            return reinterpret_cast<T*>(buf);
        }

        // checked by IsConstructorInjector concept, to help with function overload resolution
        static constexpr bool ConstructorInjector = true;

    protected:
        Properties _properties;
    private:
        ///
        /// \return
        [[nodiscard]] Task<StartBehaviour> internal_start(DependencyRegister const *_dependencies) {
            if(_serviceState != ServiceState::INSTALLED || (_dependencies != nullptr && !_dependencies->allSatisfied())) {
                INTERNAL_DEBUG("internal_start service {}:{} state {} dependencies {} {}", getServiceId(), typeName<T>(), getState(), _dependencies->size(), _dependencies->allSatisfied());
                co_return {};
            }

            INTERNAL_DEBUG("internal_start service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::STARTING);
            _serviceState = ServiceState::STARTING;
            new (buf) T();

            INTERNAL_DEBUG("internal_start service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::INJECTING);
            _serviceState = ServiceState::INJECTING;

            co_return {};
        }
        ///
        /// \return true if stopped or already stopped
        [[nodiscard]] Task<StartBehaviour> internal_stop() {
#ifdef ICHOR_USE_HARDENING
            if(_serviceState != ServiceState::UNINJECTING) [[unlikely]] {
                std::terminate();
            }
#endif

            INTERNAL_DEBUG("internal_stop service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::STOPPING);
            _serviceState = ServiceState::STOPPING;
            reinterpret_cast<T*>(buf)->~T();

            INTERNAL_DEBUG("internal_stop service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::INSTALLED);
            _serviceState = ServiceState::INSTALLED;

            co_return {};
        }

        [[nodiscard]] bool internalSetInjected() {
            if(_serviceState != ServiceState::INJECTING) {
                return false;
            }

            INTERNAL_DEBUG("internalSetInjected service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::ACTIVE);
            _serviceState = ServiceState::ACTIVE;
            return true;
        }

        [[nodiscard]] bool internalSetUninjected() {
            if(_serviceState != ServiceState::ACTIVE) {
                return false;
            }

            INTERNAL_DEBUG("internalSetUninjected service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::UNINJECTING);
            _serviceState = ServiceState::UNINJECTING;
            return true;
        }

        [[nodiscard]] ServiceState getState() const noexcept {
            return _serviceState;
        }

        void setProperties(Properties&& properties) noexcept(std::is_nothrow_move_assignable_v<Properties>) {
            _properties = std::move(properties);
        }

        ServiceIdType _serviceId;
        uint64_t _servicePriority;
        sole::uuid _serviceGid;
        ServiceState _serviceState;
        alignas(T) std::byte buf[sizeof(T)];

        friend struct DependencyRegister;

        template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires DerivedTemplated<ServiceType, AdvancedService> || IsConstructorInjector<ServiceType>
#endif
        friend class Detail::LifecycleManager;

    };
}
