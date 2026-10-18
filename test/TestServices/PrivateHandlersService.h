#pragma once

#include "UselessService.h"
#include "../TestEvents.h"
#include <ichor/DependencyManager.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/ScopedServiceProxy.h>

using namespace Ichor;
using namespace Ichor::v1;

struct IPrivateHandlersService {
    [[nodiscard]] virtual uint64_t getDependencyCount() const noexcept = 0;
    [[nodiscard]] virtual uint64_t getTrackerRequestCount() const noexcept = 0;
    [[nodiscard]] virtual uint64_t getHandledEventCount() const noexcept = 0;
    [[nodiscard]] virtual uint64_t getPreInterceptedCount() const noexcept = 0;
    [[nodiscard]] virtual uint64_t getPostInterceptedCount() const noexcept = 0;
protected:
    ~IPrivateHandlersService() = default;
};

// Keeps all functions the framework calls private and only befriends DependencyRegister and DependencyManager.
// clang 23 checks access in a concept from the concept's context, so checking these with concepts made this fail to compile.
struct PrivateHandlersService final : public IPrivateHandlersService, public AdvancedService<PrivateHandlersService> {
    PrivateHandlersService(DependencyRegister &reg, Properties props) : AdvancedService<PrivateHandlersService>(std::move(props)) {
        reg.registerDependency<IUselessService>(this, DependencyFlags::ALLOW_MULTIPLE);
    }
    ~PrivateHandlersService() final = default;

    [[nodiscard]] uint64_t getDependencyCount() const noexcept final {
        return _dependencyCount;
    }

    [[nodiscard]] uint64_t getTrackerRequestCount() const noexcept final {
        return _trackerRequestCount;
    }

    [[nodiscard]] uint64_t getHandledEventCount() const noexcept final {
        return _handledEventCount;
    }

    [[nodiscard]] uint64_t getPreInterceptedCount() const noexcept final {
        return _preInterceptedCount;
    }

    [[nodiscard]] uint64_t getPostInterceptedCount() const noexcept final {
        return _postInterceptedCount;
    }

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        _trackerRegistration = GetThreadLocalManager().registerDependencyTracker<IUselessService>(this, this);
        _handlerRegistration = GetThreadLocalManager().registerEventHandler<TestEvent>(this, this);
        _interceptorRegistration = GetThreadLocalManager().registerEventInterceptor<TestEvent>(this, this);
        co_return {};
    }

    Task<void> stop() final {
        _trackerRegistration.reset();
        _handlerRegistration.reset();
        _interceptorRegistration.reset();
        co_return;
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<IUselessService*>, IService&) {
        _dependencyCount++;
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<IUselessService*>, IService&) {
        _dependencyCount--;
    }

    AsyncGenerator<IchorBehaviour> handleDependencyRequest(v1::AlwaysNull<IUselessService*>, DependencyRequestEvent const &) {
        _trackerRequestCount++;
        co_return {};
    }

    AsyncGenerator<IchorBehaviour> handleDependencyUndoRequest(v1::AlwaysNull<IUselessService*>, DependencyUndoRequestEvent const &) {
        co_return {};
    }

    AsyncGenerator<IchorBehaviour> handleEvent(TestEvent const &) {
        _handledEventCount++;
        co_return {};
    }

    bool preInterceptEvent(TestEvent const &) {
        _preInterceptedCount++;
        return true;
    }

    void postInterceptEvent(TestEvent const &, bool processed) {
        if(processed) {
            _postInterceptedCount++;
        }
    }

    friend DependencyRegister;
    friend DependencyManager;

    DependencyTrackerRegistration _trackerRegistration{};
    EventHandlerRegistration _handlerRegistration{};
    EventInterceptorRegistration _interceptorRegistration{};
    uint64_t _dependencyCount{};
    uint64_t _trackerRequestCount{};
    uint64_t _handledEventCount{};
    uint64_t _preInterceptedCount{};
    uint64_t _postInterceptedCount{};
};
