#pragma once

#include <ichor/dependency_management/LifecycleManager.h>

extern thread_local Ichor::unordered_set<uint64_t> Ichor::Detail::emptyDependencies;

class FakeLifecycleManager final : public ILifecycleManager {
public:
    ~FakeLifecycleManager() final = default;
    std::vector<Dependency *> interestedInDependencyGoingOffline(ILifecycleManager *dependentService) noexcept override {
        return {};
    }
    StartBehaviour dependencyOnline(v1::NeverNull<ILifecycleManager *> dependentService) override {
        return {};
    }
    AsyncGenerator<StartBehaviour> dependencyOffline(v1::NeverNull<ILifecycleManager *> dependentService, std::vector<Dependency *> deps) override {
        return {};
    }
    unordered_set<ServiceIdType> &getDependencies() noexcept override {
        return Ichor::Detail::emptyDependencies;
    }
    unordered_set<ServiceIdType> &getDependees() noexcept override {
        return _serviceIdsOfDependees;
    }
    AsyncGenerator<StartBehaviour> startAfterDependencyOnline() override {
        co_return {};
    }
    AsyncGenerator<StartBehaviour> start() override {
        co_return {};
    }
    AsyncGenerator<StartBehaviour> stop() override {
        co_return {};
    }
    bool setInjected() override {
        return true;
    }
    bool setUninjected() override {
        return true;
    }
    std::string_view implementationName() const noexcept override {
        return "FakeLifecycleManager";
    }
    uint64_t type() const noexcept override {
        return 0;
    }
    ServiceIdType serviceId() const noexcept override {
        return 0;
    }
    uint64_t getPriority() const noexcept override {
        return 0;
    }
    ServiceState getServiceState() const noexcept override {
        return {};
    }
    v1::NeverNull<IService *> getIService() noexcept override {
        std::terminate();
    }
    v1::NeverNull<IService const *> getIService() const noexcept override {
        std::terminate();
    }
    std::span<Dependency const> getInterfaces() const noexcept override {
        return {};
    }
    Properties const &getProperties() const noexcept override {
        return _properties;
    }
    DependencyRegister const *getDependencyRegistry() const noexcept override {
        return _dependencyRegistry;
    }
    void insertSelfInto(uint64_t keyOfInterfaceToInject, ServiceIdType serviceIdOfOther, std::function<void(v1::NeverNull<void *>, IService &)> &) override {

    }
    void removeSelfInto(uint64_t keyOfInterfaceToInject, ServiceIdType serviceIdOfOther, std::function<void(v1::NeverNull<void *>, IService &)> &) override {

    }

    unordered_set<uint64_t> _serviceIdsOfDependees; // services that depend on this service
    Properties _properties;
    DependencyRegister *_dependencyRegistry{};
};
