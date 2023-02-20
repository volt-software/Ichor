#pragma once

#include <ichor/Concepts.h>
#include <ichor/coroutines/Task.h>
#include <ichor/dependency_management/DependencyInfo.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/dependency_management/IService.h>
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
            return fields_number_ctor<T, Ns..., sizeof...(Ns)>(0);
        }

        // This is a helper to turn a ctor into a tuple type.
        // Usage is: refl::as_tuple<data_t>
        template <typename T, typename U> struct loophole_tuple;

        template <typename T, int... Ns>
        struct loophole_tuple<T, std::integer_sequence<int, Ns...>> {
            using type = std::tuple<decltype(loophole(tag<T, Ns>{}))...>;
        };

        template <typename T>
        using as_tuple =
                typename loophole_tuple<T, std::make_integer_sequence<int, fields_number_ctor<T>(0)>>::type;

        // This is a helper to turn a ctor into a variant type.
        // Usage is: refl::as_variant<data_t>
        template <typename T, typename U> struct loophole_variant;

        template <typename T, int... Ns>
        struct loophole_variant<T, std::integer_sequence<int, Ns...>> {
            using type = std::variant<decltype(loophole(tag<T, Ns>{}))...>;
        };

        template <typename T>
        using as_variant =
                typename loophole_variant<T, std::make_integer_sequence<int, fields_number_ctor<T>(0)>>::type;

#if !defined(__clang__) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    }  // namespace refl


    class DependencyManager;

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

    extern std::atomic<uint64_t> _serviceIdCounter;

    template <class ImplT>
    concept HasConstructorInjectionDependencies = refl::fields_number_ctor<ImplT>(0) > 0;

    template <class ImplT>
    concept DoesNotHaveConstructorInjectionDependencies = refl::fields_number_ctor<ImplT>(0) == 0;

    template <typename T>
    class ConstructorInjectionService;

    template <HasConstructorInjectionDependencies T>
    class ConstructorInjectionService<T> : public IService {
    public:
        ConstructorInjectionService(DependencyRegister &reg, Properties props, DependencyManager *mng) noexcept : IService(), _properties(std::move(props)), _serviceId(_serviceIdCounter.fetch_add(1, std::memory_order_acq_rel)), _servicePriority(INTERNAL_EVENT_PRIORITY), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED), _manager(mng) {
            registerDependenciesSpecialSauce(reg, std::optional<refl::as_variant<T>>());
        }

        ~ConstructorInjectionService() noexcept override {
            _serviceId = 0;
            _serviceState = ServiceState::UNINSTALLED;
        }

        ConstructorInjectionService(const ConstructorInjectionService&) = default;
        ConstructorInjectionService(ConstructorInjectionService&&) noexcept = default;
        ConstructorInjectionService& operator=(const ConstructorInjectionService&) = default;
        ConstructorInjectionService& operator=(ConstructorInjectionService&&) noexcept = default;

        template <typename... CO_ARGS>
        void registerDependenciesSpecialSauce(DependencyRegister &reg, std::optional<std::variant<CO_ARGS...>> = {}) {
            (reg.registerDependencyConstructor<std::remove_pointer_t<CO_ARGS>>(this), ...);
        }
        template <typename... CO_ARGS>
        void createServiceSpecialSauce(std::optional<std::variant<CO_ARGS...>> = {}) {
            try {
                new(buf) T(std::get<CO_ARGS>(_deps[typeNameHash<std::remove_pointer_t<CO_ARGS>>()])...);
            } catch (std::exception const &e) {
                std::terminate();
            }
        }

        /// Process-local unique service id
        /// \return id
        [[nodiscard]] uint64_t getServiceId() const noexcept final {
            return _serviceId;
        }

        /// Global unique service id
        /// \return gid
        [[nodiscard]] sole::uuid getServiceGid() const noexcept final {
            return _serviceGid;
        }

        /// Name of the user-specified service (e.g. CoutFrameworkLogger)
        /// \return
        [[nodiscard]] std::string_view getServiceName() const noexcept final {
            return typeName<T>();
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _serviceState;
        }

        [[nodiscard]] uint64_t getServicePriority() const noexcept final {
            return _servicePriority;
        }

        [[nodiscard]] bool isStarted() const noexcept final {
            return _serviceState == ServiceState::ACTIVE;
        }

        [[nodiscard]] DependencyManager& getManager() const noexcept final {
            return *_manager;
        }

        [[nodiscard]] Properties& getProperties() noexcept final {
            return _properties;
        }

        [[nodiscard]] const Properties& getProperties() const noexcept final {
            return _properties;
        }

        [[nodiscard]] void const * getTypedServicePtr() const noexcept {
            return buf;
        }

        // checked by IsConstructorInjector concept, to help with function overload resolution
        static constexpr bool ConstructorInjector = true;

    protected:
        Properties _properties;
    private:
        ///
        /// \return true if started
        [[nodiscard]] Task<StartBehaviour> internal_start(DependencyInfo *_dependencies) {
            if(_serviceState != ServiceState::INSTALLED || (_dependencies != nullptr && !_dependencies->allSatisfied())) {
                INTERNAL_DEBUG("internal_start service {}:{} state {} dependencies {} {}", getServiceId(), typeName<T>(), getState(), _dependencies->size(), _dependencies->allSatisfied());
                co_return {};
            }

            INTERNAL_DEBUG("internal_start service {}:{} state {} -> {}", getServiceId(), typeName<T>(), getState(), ServiceState::STARTING);
            _serviceState = ServiceState::STARTING;
            createServiceSpecialSauce(std::optional<refl::as_variant<T>>());

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

        void injectDependencyManager(DependencyManager *mng) noexcept {
            _manager = mng;
        }

        void injectPriority(uint64_t priority) noexcept {
            _servicePriority = priority;
        }

        template <typename depT>
        void addDependencyInstance(depT* dep, IService *) {
            _deps.emplace(typeNameHash<depT>(), dep);
        }

        template <typename depT>
        void removeDependencyInstance(depT*, IService *) {
            _deps.erase(typeNameHash<depT>());
        }

        [[nodiscard]] T* getImplementation() noexcept {
            if(_serviceState < ServiceState::INJECTING) {
                std::terminate();
            }
            return reinterpret_cast<T*>(buf);
        }


        uint64_t _serviceId;
        uint64_t _servicePriority;
        sole::uuid _serviceGid;
        ServiceState _serviceState;
        DependencyManager *_manager{nullptr};
        unordered_map<uint64_t, refl::as_variant<T>> _deps{};
        alignas(T) std::byte buf[sizeof(T)];

        friend class DependencyManager;
        friend struct DependencyRegister;

        template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<ServiceType, IService>
#endif
        friend class DependencyLifecycleManager;

    };

    template <DoesNotHaveConstructorInjectionDependencies T>
    class ConstructorInjectionService<T> : public IService {
    public:
        ConstructorInjectionService(Properties props, DependencyManager *mng) noexcept : IService(), _properties(std::move(props)), _serviceId(_serviceIdCounter.fetch_add(1, std::memory_order_acq_rel)), _servicePriority(INTERNAL_EVENT_PRIORITY), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED), _manager(mng) {
        }

        ~ConstructorInjectionService() noexcept override {
            _serviceId = 0;
            _serviceState = ServiceState::UNINSTALLED;
        }

        ConstructorInjectionService(const ConstructorInjectionService&) = default;
        ConstructorInjectionService(ConstructorInjectionService&&) noexcept = default;
        ConstructorInjectionService& operator=(const ConstructorInjectionService&) = default;
        ConstructorInjectionService& operator=(ConstructorInjectionService&&) noexcept = default;

        /// Process-local unique service id
        /// \return id
        [[nodiscard]] uint64_t getServiceId() const noexcept final {
            return _serviceId;
        }

        /// Global unique service id
        /// \return gid
        [[nodiscard]] sole::uuid getServiceGid() const noexcept final {
            return _serviceGid;
        }

        /// Name of the user-specified service (e.g. CoutFrameworkLogger)
        /// \return
        [[nodiscard]] std::string_view getServiceName() const noexcept final {
            return typeName<T>();
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _serviceState;
        }

        [[nodiscard]] uint64_t getServicePriority() const noexcept final {
            return _servicePriority;
        }

        [[nodiscard]] bool isStarted() const noexcept final {
            return _serviceState == ServiceState::ACTIVE;
        }

        [[nodiscard]] DependencyManager& getManager() const noexcept final {
            return *_manager;
        }

        [[nodiscard]] Properties& getProperties() noexcept final {
            return _properties;
        }

        [[nodiscard]] const Properties& getProperties() const noexcept final {
            return _properties;
        }

        [[nodiscard]] void const * getTypedServicePtr() const noexcept {
            return buf;
        }

        [[nodiscard]] T* getImplementation() noexcept {
            return reinterpret_cast<T*>(buf);
        }

        // checked by IsConstructorInjector concept, to help with function overload resolution
        static constexpr bool ConstructorInjector = true;

    protected:
        Properties _properties;
    private:
        ///
        /// \return true if started
        [[nodiscard]] Task<StartBehaviour> internal_start(DependencyInfo *_dependencies) {
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

        void injectDependencyManager(DependencyManager *mng) noexcept {
            _manager = mng;
        }

        void injectPriority(uint64_t priority) noexcept {
            _servicePriority = priority;
        }

        uint64_t _serviceId;
        uint64_t _servicePriority;
        sole::uuid _serviceGid;
        ServiceState _serviceState;
        DependencyManager *_manager{nullptr};
        alignas(T) std::byte buf[sizeof(T)];

        friend class DependencyManager;
        friend struct DependencyRegister;

        template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires DerivedTemplated<ServiceType, AdvancedService> || IsConstructorInjector<ServiceType>
#endif
        friend class LifecycleManager;

    };
}

#ifndef ICHOR_DEPENDENCY_MANAGER
#include <ichor/DependencyManager.h>
#endif
