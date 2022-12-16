#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/Service.h>
#include "ichor/dependency_management/ILifecycleManager.h"
#include "AwaitService.h"

using namespace Ichor;

class AsyncUsingTimerService final : public Service<AsyncUsingTimerService> {
public:
    AsyncUsingTimerService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<IAwaitService>(this, true);
    }
    ~AsyncUsingTimerService() final = default;

    AsyncGenerator<void> start() final {
        fmt::print("start\n");
        _timerManager = getManager().createServiceManager<Timer, ITimer>();
        _timerManager->setChronoInterval(100ms);
        _timerManager->setCallbackAsync(this, [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
            fmt::print("timer 1\n");
            co_await _awaitSvc->await_something().begin();
            fmt::print("timer 2\n");
            _timerManager->stopTimer();
            getManager().pushEvent<QuitEvent>(getServiceId());

            co_return {};
        });
        _timerManager->startTimer();
        co_return;
    }

    AsyncGenerator<void> stop() final {
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
    friend DependencyManager;
};
