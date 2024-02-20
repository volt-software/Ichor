#pragma once

#include <string_view>
#include <memory>
#include "AdvancedService.h"
#include <ichor/Common.h>
#include <ichor/stl/StaticVector.h>
#include "Dependency.h"
#include "DependencyRegister.h"

namespace Ichor {
    /// Type erasure abstract class.
    class ILifecycleManager {
    public:
        virtual ~ILifecycleManager() = default;
        virtual std::vector<Dependency*> interestedInDependencyGoingOffline(ILifecycleManager *dependentService) noexcept = 0;
        virtual StartBehaviour dependencyOnline(NeverNull<ILifecycleManager*> dependentService) = 0;
        // iterators come from interestedInDependency() and have to be moved as using coroutines might end up clearing it.
        virtual AsyncGenerator<StartBehaviour> dependencyOffline(NeverNull<ILifecycleManager*> dependentService, std::vector<Dependency*> deps) = 0;
        [[nodiscard]] virtual unordered_set<ServiceIdType> &getDependencies() noexcept = 0;
        [[nodiscard]] virtual unordered_set<ServiceIdType> &getDependees() noexcept = 0;
        [[nodiscard]] virtual AsyncGenerator<StartBehaviour> startAfterDependencyOnline() = 0;
        [[nodiscard]] virtual AsyncGenerator<StartBehaviour> start() = 0;
        [[nodiscard]] virtual AsyncGenerator<StartBehaviour> stop() = 0;
        [[nodiscard]] virtual bool setInjected() = 0;
        [[nodiscard]] virtual bool setUninjected() = 0;
        [[nodiscard]] virtual std::string_view implementationName() const noexcept = 0;
        [[nodiscard]] virtual uint64_t type() const noexcept = 0;
        [[nodiscard]] virtual ServiceIdType serviceId() const noexcept = 0;
        [[nodiscard]] virtual uint64_t getPriority() const noexcept = 0;
        [[nodiscard]] virtual ServiceState getServiceState() const noexcept = 0;
        [[nodiscard]] virtual NeverNull<IService*> getIService() noexcept = 0;
        [[nodiscard]] virtual NeverNull<IService const*> getIService() const noexcept = 0;
        [[nodiscard]] virtual const IStaticVector<Dependency>& getInterfaces() const noexcept = 0;
        [[nodiscard]] virtual Properties const & getProperties() const noexcept = 0;
        [[nodiscard]] virtual DependencyRegister const * getDependencyRegistry() const noexcept = 0;
        virtual void insertSelfInto(uint64_t keyOfInterfaceToInject, ServiceIdType serviceIdOfOther, std::function<void(NeverNull<void*>, IService&)>&) = 0;
        virtual void removeSelfInto(uint64_t keyOfInterfaceToInject, ServiceIdType serviceIdOfOther, std::function<void(NeverNull<void*>, IService&)>&) = 0;

    protected:
        static Task<void> waitForService(ServiceIdType serviceId, uint64_t eventType) noexcept;
    };
}

// Implementations of ILifecycleManager
#include <ichor/dependency_management/DependencyLifecycleManager.h>
#include <ichor/dependency_management/LifecycleManager.h>
