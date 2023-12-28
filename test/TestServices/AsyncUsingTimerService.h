#pragma once

#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/dependency_management/AdvancedService.h>
#include "AwaitService.h"

using namespace Ichor;

class AsyncUsingTimerService final : public AdvancedService<AsyncUsingTimerService> {
public:
    AsyncUsingTimerService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<IAwaitService>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<ITimerFactory>(this, DependencyFlags::REQUIRED);
    }
    ~AsyncUsingTimerService() final = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        fmt::print("start\n");
        auto &timer = _timerFactory->createTimer();
        timer.setChronoInterval(100ms);
        timer.setCallbackAsync([this, &timer = timer]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("timer 1\n");
            co_await _awaitSvc->await_something();
            fmt::print("timer 2\n");
            timer.stopTimer();
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

            co_return {};
        });
        timer.startTimer();
        co_return {};
    }

    Task<void> stop() final {
        co_return;
    }

    void addDependencyInstance(IAwaitService &svc, IService&) {
        _awaitSvc = &svc;
    }

    void removeDependencyInstance(IAwaitService&, IService&) {
        _awaitSvc = nullptr;
    }

    void addDependencyInstance(ITimerFactory &factory, IService &) {
        _timerFactory = &factory;
    }

    void removeDependencyInstance(ITimerFactory &factory, IService&) {
        _timerFactory = nullptr;
    }

private:
    IAwaitService* _awaitSvc{};
    ITimerFactory *_timerFactory{};

    friend DependencyRegister;
};
