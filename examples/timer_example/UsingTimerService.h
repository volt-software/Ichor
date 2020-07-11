#pragma once

#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <optional_bundles/logging_bundle/Logger.h>
#include <optional_bundles/timer_bundle/TimerService.h>
#include "framework/Service.h"
#include "framework/ServiceLifecycleManager.h"

using namespace Cppelix;


struct IUsingTimerService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class UsingTimerService : public IUsingTimerService, public Service {
public:
    ~UsingTimerService() final = default;

    bool start() final {
        LOG_INFO(_logger, "UsingTimerService started");
        _timerEventRegistration = _manager->registerEventHandler<TimerEvent>(getServiceId(), this);
        _timerManager = _manager->createServiceManager<ITimer, Timer>();
        _timerManager->setChronoInterval(std::chrono::milliseconds(500));
        _timerManager->startTimer();
        return true;
    }

    bool stop() final {
        _timerEventRegistration = nullptr;
        _timerManager = nullptr;
        LOG_INFO(_logger, "UsingTimerService stopped");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    bool handleEvent(TimerEvent const * const evt) {
        if(evt->timerId != _timerManager->timerId()) {
            // let another timer handler deal with it
            return false;
        }

        _timerTriggerCount++;
        LOG_INFO(_logger, "Timer triggered {} times", _timerTriggerCount);
        if(_timerTriggerCount == 5) {
            _manager->pushEventThreadUnsafe<QuitEvent>(getServiceId());
        }

        // we dealt with it, don't propagate to other handlers
        return true;
    }

private:
    ILogger *_logger{nullptr};
    std::unique_ptr<EventHandlerRegistration> _timerEventRegistration{nullptr};
    uint64_t _timerTriggerCount{0};
    Timer* _timerManager{nullptr};
};