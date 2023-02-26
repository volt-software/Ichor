#pragma once

#include <ichor/services/timer/TimerService.h>
#include <ichor/dependency_management/AdvancedService.h>
#include "AwaitService.h"

using namespace Ichor;

class AsyncUsingTimerService final : public AdvancedService<AsyncUsingTimerService> {
public:
    AsyncUsingTimerService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<IAwaitService>(this, true);
    }
    ~AsyncUsingTimerService() final = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        fmt::print("start\n");
        _timerManager = GetThreadLocalManager().createServiceManager<Timer, ITimer>();
        _timerManager->setChronoInterval(100ms);
        _timerManager->setCallbackAsync(this, [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
            fmt::print("timer 1\n");
            co_await _awaitSvc->await_something();
            fmt::print("timer 2\n");
            _timerManager->stopTimer();
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

            co_return {};
        });
        _timerManager->startTimer();
        co_return {};
    }

    Task<void> stop() final {
        _timerManager = nullptr;
        co_return;
    }

    void addDependencyInstance(IAwaitService *svc, IService *) {
        _awaitSvc = svc;
    }

    void removeDependencyInstance(IAwaitService *, IService *) {
        _awaitSvc = nullptr;
    }

private:
    Timer* _timerManager{};
    IAwaitService* _awaitSvc{};

    friend DependencyRegister;
};
