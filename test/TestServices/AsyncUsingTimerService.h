#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include "AwaitService.h"

using namespace Ichor;

class AsyncUsingTimerService final : public Service<AsyncUsingTimerService> {
public:
    AsyncUsingTimerService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<IAwaitService>(this, true);
    }
    ~AsyncUsingTimerService() final = default;

    StartBehaviour start() final {
        fmt::print("start\n");
        _timerManager = getManager().createServiceManager<Timer, ITimer>();
        _timerManager->setChronoInterval(std::chrono::milliseconds(100));
        _timerManager->setCallback(this, [this](DependencyManager &dm) -> AsyncGenerator<void> {
            co_await _awaitSvc->await_something().begin();
            _timerManager->stopTimer();
            getManager().pushEvent<QuitEvent>(getServiceId());

            co_return;
        });
        _timerManager->startTimer();
        return StartBehaviour::SUCCEEDED;
    }

    void addDependencyInstance(IAwaitService *svc, IService *) {
        _awaitSvc = svc;
    }

    void removeDependencyInstance(IAwaitService *, IService *) {
        _awaitSvc = nullptr;
    }

    StartBehaviour stop() final {
        _timerManager = nullptr;
        return StartBehaviour::SUCCEEDED;
    }

private:
    Timer* _timerManager{};
    IAwaitService* _awaitSvc{};
};