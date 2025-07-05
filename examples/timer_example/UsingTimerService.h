#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>

using namespace Ichor;
using namespace Ichor::v1;

class UsingTimerService final {
public:
    UsingTimerService(DependencyManager *dm, IService *self, ILogger *logger, ITimerFactory *timerFactory) : _dm(dm), _self(self), _logger(logger), _timer(&timerFactory->createTimer()) {
        ICHOR_LOG_INFO(_logger, "UsingTimerService started");
        _timer->setChronoInterval(std::chrono::milliseconds(50));
        _timer->setCallback([this]() {
            handleEvent();
        });
        _timer->startTimer();
    }
    ~UsingTimerService() {
        ICHOR_LOG_INFO(_logger, "UsingTimerService stopped");
    }

private:
    void handleEvent() {
        _timerTriggerCount++;
        ICHOR_LOG_INFO(_logger, "Timer {} triggered {} times", _timer->getTimerId(), _timerTriggerCount);
        if(_timerTriggerCount == 5) {
            _dm->getEventQueue().pushEvent<QuitEvent>(_self->getServiceId());
        }
    }


    DependencyManager *_dm{};
    IService *_self{};
    ILogger *_logger{};
    ITimer *_timer{};
    uint64_t _timerTriggerCount{0};
};
