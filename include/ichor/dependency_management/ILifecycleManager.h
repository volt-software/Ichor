#pragma once

#include <string_view>
#include <span>
#include <ichor/Common.h>
#include <ichor/coroutines/Task.h>
#include "Dependency.h"
#include "DependencyRegister.h"

namespace Ichor {
    /// Type erasure abstract class.
    class ILifecycleManager {
    public:
        virtual ~ILifecycleManager() = default;
        virtual std::vector<Dependency*> interestedInDependencyGoingOffline(ILifecycleManager *dependentService) noexcept = 0;
        virtual StartBehaviour dependencyOnline(v1::NeverNull<ILifecycleManager*> dependentService) = 0;
        // iterators come from interestedInDependency() and have to be moved as using coroutines might end up clearing it.
        virtual AsyncGenerator<StartBehaviour> dependencyOffline(v1::NeverNull<ILifecycleManager*> dependentService, std::vector<Dependency*> deps) = 0;
        [[nodiscard]] virtual unordered_set<ServiceIdType, ServiceIdHash> &getDependencies() noexcept = 0;
        [[nodiscard]] virtual unordered_set<ServiceIdType, ServiceIdHash> &getDependees() noexcept = 0;
        [[nodiscard]] virtual AsyncGenerator<StartBehaviour> startAfterDependencyOnline() = 0;
        [[nodiscard]] virtual AsyncGenerator<StartBehaviour> start() = 0;
        [[nodiscard]] virtual AsyncGenerator<StartBehaviour> stop() = 0;
        [[nodiscard]] virtual bool setInjected() = 0;
        [[nodiscard]] virtual bool setUninjected() = 0;
        [[nodiscard]] ICHOR_PURE_FUNC_ATTR virtual std::string_view implementationName() const noexcept = 0;
        [[nodiscard]] ICHOR_PURE_FUNC_ATTR virtual uint64_t type() const noexcept = 0;
        [[nodiscard]] ICHOR_PURE_FUNC_ATTR virtual ServiceIdType serviceId() const noexcept = 0;
        [[nodiscard]] virtual uint64_t getPriority() const noexcept = 0;
        [[nodiscard]] virtual ServiceState getServiceState() const noexcept = 0;
        [[nodiscard]] virtual v1::NeverNull<IService*> getIService() noexcept = 0;
        [[nodiscard]] virtual v1::NeverNull<IService const*> getIService() const noexcept = 0;
        [[nodiscard]] virtual std::span<Dependency const> getInterfaces() const noexcept = 0;
        [[nodiscard]] virtual Properties const & getProperties() const noexcept = 0;
        [[nodiscard]] virtual DependencyRegister const * getDependencyRegistry() const noexcept = 0;
        [[nodiscard]] virtual bool isInternalManager() const noexcept = 0;
        virtual void insertSelfInto(uint64_t keyOfInterfaceToInject, ServiceIdType serviceIdOfOther, std::function<void(v1::NeverNull<void*>, IService&)>&) = 0;
        virtual void removeSelfInto(uint64_t keyOfInterfaceToInject, ServiceIdType serviceIdOfOther, std::function<void(v1::NeverNull<void*>, IService&)>&) = 0;

    protected:
        static Task<void> waitForService(ServiceIdType serviceId, uint64_t eventType) noexcept;
    };
}
