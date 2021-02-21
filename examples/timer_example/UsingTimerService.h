#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;

class UsingTimerService final : public Service<UsingTimerService> {
public:
    UsingTimerService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~UsingTimerService() final = default;

    bool start() final {
        ICHOR_LOG_INFO(_logger, "UsingTimerService started");
        _timerManager = getManager()->createServiceManager<Timer, ITimer>();
        _timerManager->setChronoInterval(std::chrono::milliseconds(500));
        _timerEventRegistration = getManager()->registerEventHandler<TimerEvent>(this, _timerManager->getServiceId());
        _timerManager->startTimer();
        return true;
    }

    bool stop() final {
        _timerEventRegistration.reset();
        _timerManager = nullptr;
        ICHOR_LOG_INFO(_logger, "UsingTimerService stopped");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    Generator<bool> handleEvent(TimerEvent const * const evt) {
        _timerTriggerCount++;
        ICHOR_LOG_INFO(_logger, "Timer {} triggered {} times", _timerManager->getServiceId(), _timerTriggerCount);
        if(_timerTriggerCount == 5) {
            getManager()->pushEvent<QuitEvent>(getServiceId());
        }

        co_return (bool)PreventOthersHandling;
    }

private:
    ILogger *_logger{nullptr};
    std::unique_ptr<EventHandlerRegistration, Deleter> _timerEventRegistration{nullptr};
    uint64_t _timerTriggerCount{0};
    Timer* _timerManager{nullptr};
};