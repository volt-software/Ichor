#pragma once

#include <string_view>
#include <memory>
#include <ichor/Service.h>
#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/Common.h>
#include <ichor/events/Event.h>
#include <ichor/Dependency.h>
#include <ichor/DependencyInfo.h>
#include <ichor/DependencyRegister.h>

namespace Ichor {

    class ILifecycleManager {
    public:
        virtual ~ILifecycleManager() = default;
        ///
        /// \param dependentService
        /// \return true if dependency is registered in service, false if not
        virtual bool dependencyOnline(ILifecycleManager* dependentService) = 0;
        ///
        /// \param dependentService
        /// \return true if dependency is registered in service, false if not
        virtual bool dependencyOffline(ILifecycleManager* dependentService) = 0;
        [[nodiscard]] virtual StartBehaviour start() = 0;
        [[nodiscard]] virtual StartBehaviour stop() = 0;
        [[nodiscard]] virtual bool setInjected() = 0;
        [[nodiscard]] virtual bool setUninjected() = 0;
        [[nodiscard]] virtual std::string_view implementationName() const noexcept = 0;
        [[nodiscard]] virtual uint64_t type() const noexcept = 0;
        [[nodiscard]] virtual uint64_t serviceId() const noexcept = 0;
        [[nodiscard]] virtual uint64_t getPriority() const noexcept = 0;
        [[nodiscard]] virtual ServiceState getServiceState() const noexcept = 0;
        [[nodiscard]] virtual const std::vector<Dependency>& getInterfaces() const noexcept = 0;
        [[nodiscard]] virtual Properties const & getProperties() const noexcept = 0;
        [[nodiscard]] virtual DependencyRegister const * getDependencyRegistry() const noexcept = 0;
        virtual void insertSelfInto(uint64_t keyOfInterfaceToInject, std::function<void(void*, IService*)>&) = 0;
    };

    template<class ServiceType, typename... IFaces>
    requires DerivedTemplated<ServiceType, Service>
    class DependencyLifecycleManager final : public ILifecycleManager {
    public:
        explicit DependencyLifecycleManager(IFrameworkLogger *logger, std::string_view name, std::vector<Dependency> interfaces, Properties&& properties, DependencyManager *mng) : _implementationName(name), _interfaces(std::move(interfaces)), _registry(mng), _dependencies(), _service(_registry, std::forward<Properties>(properties), mng), _logger(logger) {
            for(auto const &reg : _registry._registrations) {
                _dependencies.addDependency(std::get<0>(reg.second));
            }
        }

        ~DependencyLifecycleManager() final {
            ICHOR_LOG_TRACE(_logger, "destroying {}, id {}", typeName<ServiceType>(), _service.getServiceId());
            for(auto const &dep : _dependencies._dependencies) {
                // _manager is always injected in DependencyManager::create...Manager functions.
                _service._manager->template pushPrioritisedEvent<DependencyUndoRequestEvent>(_service.getServiceId(), getPriority(), Dependency{dep.interfaceNameHash, dep.required, dep.satisfied}, &getProperties());
            }
        }

        template<typename... Interfaces>
        [[nodiscard]]
        static std::shared_ptr<DependencyLifecycleManager<ServiceType, Interfaces...>> create(IFrameworkLogger *logger, std::string_view name, Properties&& properties, DependencyManager *mng, InterfacesList_t<Interfaces...>) {
            if (name.empty()) {
                name = typeName<ServiceType>();
            }

            std::vector<Dependency> interfaces{};
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), false, false),...);
            return std::make_shared<DependencyLifecycleManager<ServiceType, Interfaces...>>(logger, name, std::move(interfaces), std::forward<Properties>(properties), mng);
        }

        bool dependencyOnline(ILifecycleManager* dependentService) final {
            bool interested = false;
            auto const &interfaces = dependentService->getInterfaces();

            for(auto const &interface : interfaces) {
                auto dep = _dependencies.find(interface);
                if (dep == _dependencies.end()) {
                    continue;
                }

                if(dep->satisfied == 0) {
                    interested = true;
                }
                injectIntoSelfDoubleDispatch(interface.interfaceNameHash, dependentService);
                dep->satisfied++;
            }

            return interested;
        }

        void injectIntoSelfDoubleDispatch(uint64_t keyOfInterfaceToInject, ILifecycleManager* dependentService) {
            auto dep = _registry._registrations.find(keyOfInterfaceToInject);

            if(dep != end(_registry._registrations)) {
                dependentService->insertSelfInto(keyOfInterfaceToInject, std::get<1>(dep->second));
            }
        }

        void insertSelfInto(uint64_t keyOfInterfaceToInject, std::function<void(void*, IService*)> &fn) final {
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
            }
        }

        template <int i, typename Iface1, typename... otherIfaces>
        void insertSelfInto2(uint64_t keyOfInterfaceToInject, std::function<void(void*, IService*)> &fn) {
            if(typeNameHash<Iface1>() == keyOfInterfaceToInject) {
                fn(static_cast<Iface1*>(&_service), static_cast<IService*>(&_service));
            } else {
                if constexpr(i > 1) {
                    insertSelfInto2<sizeof...(otherIfaces), otherIfaces...>(keyOfInterfaceToInject, fn);
                }
            }
        }

        bool dependencyOffline(ILifecycleManager* dependentService) final {
            auto const &interfaces = dependentService->getInterfaces();
            bool interested = false;

            for(auto const &interface : interfaces) {
                auto dep = _dependencies.find(interface);
                if (dep == _dependencies.end()) {
                    continue;
                }

                // dependency should not be marked as unsatisified if there is at least one other of the same type present
                dep->satisfied--;
                if (dep->required && dep->satisfied == 0) {
                    interested = true;
                }

                removeSelfIntoDoubleDispatch(interface.interfaceNameHash, dependentService);
            }

            return interested;
        }

        void removeSelfIntoDoubleDispatch(uint64_t keyOfInterfaceToInject, ILifecycleManager* dependentService) {
            auto dep = _registry._registrations.find(keyOfInterfaceToInject);

            if(dep != end(_registry._registrations)) {
                dependentService->insertSelfInto(keyOfInterfaceToInject, std::get<2>(dep->second));
            }
        }

        [[nodiscard]]
        StartBehaviour start() final {
            bool canStart = _service.getState() != ServiceState::ACTIVE && _dependencies.allSatisfied();
            StartBehaviour ret = StartBehaviour::FAILED_DO_NOT_RETRY;
            if (canStart) {
                ret = _service.internal_start();
                if(ret == StartBehaviour::SUCCEEDED) {
                    ICHOR_LOG_DEBUG(_logger, "Started {}", _implementationName);
                } else {
                    ICHOR_LOG_DEBUG(_logger, "Couldn't start {} {}", serviceId(), _implementationName);
                }
            }

            return ret;
        }

        [[nodiscard]]
        StartBehaviour stop() final {
            auto ret = _service.internal_stop();

//            if(ret == StartBehaviour::SUCCEEDED) {
//                ICHOR_LOG_DEBUG(_logger, "Stopped {}", _implementationName);
//            } else {
//                ICHOR_LOG_DEBUG(_logger, "Couldn't stop {} {}", serviceId(), _implementationName);
//            }

            return ret;
        }

        [[nodiscard]]
        bool setInjected() final {
            return _service.internalSetInjected();
        }

        [[nodiscard]]
        bool setUninjected() final {
            return _service.internalSetUninjected();
        }

        [[nodiscard]] std::string_view implementationName() const noexcept final {
            return _implementationName;
        }

        [[nodiscard]] uint64_t type() const noexcept final {
            return typeNameHash<ServiceType>();
        }

        [[nodiscard]] uint64_t serviceId() const noexcept final {
            return _service.getServiceId();
        }

        [[nodiscard]] uint64_t getPriority() const noexcept final {
            return _service.getServicePriority();
        }

        [[nodiscard]] ServiceType& getService() noexcept {
            return _service;
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _service.getState();
        }

        [[nodiscard]] const std::vector<Dependency>& getInterfaces() const noexcept final {
            return _interfaces;
        }

        [[nodiscard]] Properties const & getProperties() const noexcept final {
            return _service._properties;
        }

        [[nodiscard]] DependencyRegister const * getDependencyRegistry() const noexcept final {
            return &_registry;
        }

    private:
        const std::string_view _implementationName;
        std::vector<Dependency> _interfaces;
        DependencyRegister _registry;
        DependencyInfo _dependencies;
//        std::vector<uint64_t> _injectedDependencies;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };

    template<class ServiceType, typename... IFaces>
    requires DerivedTemplated<ServiceType, Service>
    class LifecycleManager final : public ILifecycleManager {
    public:
        template <typename U = ServiceType> requires RequestsProperties<U>
        explicit LifecycleManager(IFrameworkLogger *logger, std::string_view name, std::vector<Dependency> interfaces, Properties&& properties, DependencyManager *mng) : _implementationName(name), _interfaces(std::move(interfaces)), _service(std::forward<Properties>(properties), mng), _logger(logger) {
        }

        template <typename U = ServiceType> requires (!RequestsProperties<U>)
        explicit LifecycleManager(IFrameworkLogger *logger, std::string_view name, std::vector<Dependency> interfaces, Properties&& properties, DependencyManager *mng) : _implementationName(name), _interfaces(std::move(interfaces)), _service(), _logger(logger) {
            _service.setProperties(std::forward<Properties>(properties));
        }

        ~LifecycleManager() final = default;

        template<typename... Interfaces>
        [[nodiscard]]
        static std::shared_ptr<LifecycleManager<ServiceType, Interfaces...>> create(IFrameworkLogger *logger, std::string_view name, Properties&& properties, DependencyManager *mng, InterfacesList_t<Interfaces...>) {
            if (name.empty()) {
                name = typeName<ServiceType>();
            }

            std::vector<Dependency> interfaces{};
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), false, false),...);
            return std::make_shared<LifecycleManager<ServiceType, Interfaces...>>(logger, name, std::move(interfaces), std::forward<Properties>(properties), mng);
        }

        bool dependencyOnline(ILifecycleManager* dependentService) final {
            return false;
        }

        bool dependencyOffline(ILifecycleManager* dependentService) final {
            return false;
        }

        [[nodiscard]]
        StartBehaviour start() final {
            auto ret = _service.internal_start();

            if(ret == StartBehaviour::SUCCEEDED) {
                ICHOR_LOG_DEBUG(_logger, "Started {}", _implementationName);
            } else {
                ICHOR_LOG_DEBUG(_logger, "Couldn't start {} {}", serviceId(), _implementationName);
            }

            return ret;
        }

        [[nodiscard]]
        StartBehaviour stop() final {
            auto ret = _service.internal_stop();

            if(ret == StartBehaviour::SUCCEEDED) {
                ICHOR_LOG_DEBUG(_logger, "Stopped {}", _implementationName);
            } else {
                ICHOR_LOG_DEBUG(_logger, "Couldn't stop {} {}", serviceId(), _implementationName);
            }

            return ret;
        }

        [[nodiscard]]
        bool setInjected() final {
            return _service.internalSetInjected();
        }

        [[nodiscard]]
        bool setUninjected() final {
            return _service.internalSetUninjected();
        }

        [[nodiscard]] std::string_view implementationName() const noexcept final {
            return _implementationName;
        }

        [[nodiscard]] uint64_t type() const noexcept final {
            return typeNameHash<ServiceType>();
        }

        [[nodiscard]] uint64_t serviceId() const noexcept final {
            return _service.getServiceId();
        }

        [[nodiscard]] ServiceType& getService() noexcept {
            return _service;
        }

        [[nodiscard]] uint64_t getPriority() const noexcept final {
            return _service.getServicePriority();
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _service.getState();
        }

        [[nodiscard]] const std::vector<Dependency>& getInterfaces() const noexcept final {
            return _interfaces;
        }

        [[nodiscard]] Properties const & getProperties() const noexcept final {
            return _service._properties;
        }

        [[nodiscard]] DependencyRegister const * getDependencyRegistry() const noexcept final {
            return nullptr;
        }

        void insertSelfInto(uint64_t keyOfInterfaceToInject, std::function<void(void*, IService*)> &fn) final {
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
            }
        }

        template <int i, typename Iface1, typename... otherIfaces>
        void insertSelfInto2(uint64_t keyOfInterfaceToInject, std::function<void(void*, IService*)> &fn) {
            if(typeNameHash<Iface1>() == keyOfInterfaceToInject) {
                fn(static_cast<Iface1*>(&_service), static_cast<IService*>(&_service));
            } else {
                if constexpr(i > 1) {
                    insertSelfInto2<sizeof...(otherIfaces), otherIfaces...>(keyOfInterfaceToInject, fn);
                }
            }
        }

    private:
        const std::string_view _implementationName;
        std::vector<Dependency> _interfaces;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };
}
