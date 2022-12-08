#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/Service.h>
#include "ichor/dependency_management/ILifecycleManager.h"

using namespace Ichor;

class UsingTimerService final : public Service<UsingTimerService> {
public:
    UsingTimerService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~UsingTimerService() final = default;

private:
    AsyncGenerator<void> start() final {
        ICHOR_LOG_INFO(_logger, "UsingTimerService started");
        _timerManager = getManager().createServiceManager<Timer, ITimer>();
        _timerManager->setChronoInterval(std::chrono::milliseconds(50));
        _timerManager->setCallback(this, [this](DependencyManager &dm) {
            return handleEvent(dm);
        });
        _timerManager->startTimer();
        co_return;
    }

    AsyncGenerator<void> stop() final {
        _timerManager = nullptr;
        ICHOR_LOG_INFO(_logger, "UsingTimerService stopped");
        co_return;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    AsyncGenerator<IchorBehaviour> handleEvent(DependencyManager &dm) {
        _timerTriggerCount++;
        ICHOR_LOG_INFO(_logger, "Timer {} triggered {} times", _timerManager->getServiceId(), _timerTriggerCount);
        if(_timerTriggerCount == 5) {
            getManager().pushEvent<QuitEvent>(getServiceId());
        }

        co_return {};
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{nullptr};
    uint64_t _timerTriggerCount{0};
    Timer* _timerManager{nullptr};
};