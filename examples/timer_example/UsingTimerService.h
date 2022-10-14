#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;

class UsingTimerService final : public Service<UsingTimerService> {
public:
    UsingTimerService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~UsingTimerService() final = default;

    StartBehaviour start() final {
        ICHOR_LOG_INFO(_logger, "UsingTimerService started");
        _timerManager = getManager()->createServiceManager<Timer, ITimer>();
        _timerManager->setChronoInterval(std::chrono::milliseconds(50));
        _timerManager->setCallback([this](TimerEvent const * const evt) {
            return handleEvent(evt);
        });
        _timerManager->startTimer();
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        _timerManager = nullptr;
        ICHOR_LOG_INFO(_logger, "UsingTimerService stopped");
        return StartBehaviour::SUCCEEDED;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    AsyncGenerator<bool> handleEvent(TimerEvent const * const evt) {
        _timerTriggerCount++;
        ICHOR_LOG_INFO(_logger, "Timer {} triggered {} times", _timerManager->getServiceId(), _timerTriggerCount);
        if(_timerTriggerCount == 5) {
            getManager()->pushEvent<QuitEvent>(getServiceId());
        }

        co_return (bool)PreventOthersHandling;
    }

private:
    ILogger *_logger{nullptr};
    uint64_t _timerTriggerCount{0};
    Timer* _timerManager{nullptr};
};