#pragma once

#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/dependency_management/AdvancedService.h>
#include "AwaitService.h"
#include <ichor/ServiceExecutionScope.h>

using namespace Ichor;
using namespace Ichor::v1;

class AsyncUsingTimerService final : public AdvancedService<AsyncUsingTimerService> {
public:
    AsyncUsingTimerService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<IAwaitService>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<ITimerFactory>(this, DependencyFlags::REQUIRED);
    }
    ~AsyncUsingTimerService() final = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        fmt::print("start\n");
        auto timer = _timerFactory->createTimer();
        timer.setChronoInterval(100ms);
        timer.setCallbackAsync([this, timer]() mutable -> AsyncGenerator<IchorBehaviour> {
            fmt::print("timer 1\n");
            co_await _awaitSvc->await_something();
            fmt::print("timer 2\n");
            timer.stopTimer({});
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

            co_return {};
        });
        timer.startTimer();
        co_return {};
    }

    Task<void> stop() final {
        co_return;
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<IAwaitService*> svc, IService&) {
        _awaitSvc = std::move(svc);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<IAwaitService*>, IService&) {
        _awaitSvc.reset();
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> factory, IService &) {
        _timerFactory = std::move(factory);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> factory, IService&) {
        _timerFactory.reset();
    }

private:
    Ichor::ScopedServiceProxy<IAwaitService*> _awaitSvc {};
    Ichor::ScopedServiceProxy<ITimerFactory*> _timerFactory {};

    friend DependencyRegister;
};
