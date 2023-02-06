#pragma once

#include <string_view>
#include <memory>
#include "AdvancedService.h"
#include <ichor/Common.h>
#include "Dependency.h"
#include "DependencyRegister.h"

namespace Ichor {
    /// Type erasure abstract class.
    class ILifecycleManager {
    public:
        virtual ~ILifecycleManager() = default;
        virtual std::vector<decltype(std::declval<DependencyInfo>().begin())> interestedInDependency(ILifecycleManager *dependentService, bool online) noexcept = 0;
        // iterators come from interestedInDependency() and have to be moved as using coroutines might end up clearing it.
        virtual AsyncGenerator<StartBehaviour> dependencyOnline(ILifecycleManager* dependentService, std::vector<decltype(std::declval<DependencyInfo>().begin())> iterators) = 0;
        virtual AsyncGenerator<StartBehaviour> dependencyOffline(ILifecycleManager* dependentService, std::vector<decltype(std::declval<DependencyInfo>().begin())> iterators) = 0;
        [[nodiscard]] virtual unordered_set<uint64_t> &getDependencies() noexcept = 0;
        [[nodiscard]] virtual unordered_set<uint64_t> &getDependees() noexcept = 0;
        [[nodiscard]] virtual AsyncGenerator<StartBehaviour> start() = 0;
        [[nodiscard]] virtual AsyncGenerator<StartBehaviour> stop() = 0;
        [[nodiscard]] virtual bool setInjected() = 0;
        [[nodiscard]] virtual bool setUninjected() = 0;
        [[nodiscard]] virtual std::string_view implementationName() const noexcept = 0;
        [[nodiscard]] virtual uint64_t type() const noexcept = 0;
        [[nodiscard]] virtual uint64_t serviceId() const noexcept = 0;
        [[nodiscard]] virtual uint64_t getPriority() const noexcept = 0;
        [[nodiscard]] virtual ServiceState getServiceState() const noexcept = 0;
        [[nodiscard]] virtual IService const * getIService() const noexcept = 0;
        [[nodiscard]] virtual void const * getTypedServicePtr() const noexcept = 0;
        [[nodiscard]] virtual const std::vector<Dependency>& getInterfaces() const noexcept = 0;
        [[nodiscard]] virtual Properties const & getProperties() const noexcept = 0;
        [[nodiscard]] virtual DependencyRegister const * getDependencyRegistry() const noexcept = 0;
        virtual void insertSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)>&) = 0;
        virtual void removeSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)>&) = 0;

    protected:
        static Ichor::AsyncGenerator<void> waitForService(DependencyManager &dm, uint64_t serviceId, uint64_t eventType) noexcept;
    };
}

// Implementations of ILifecycleManager
#include <ichor/dependency_management/DependencyLifecycleManager.h>
#include <ichor/dependency_management/LifecycleManager.h>
