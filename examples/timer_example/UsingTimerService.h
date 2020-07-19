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

class UsingTimerService final : public IUsingTimerService, public Service {
public:
    ~UsingTimerService() final = default;

    bool start() final {
        LOG_INFO(_logger, "UsingTimerService started");
        _timerManager = getManager()->createServiceManager<ITimer, Timer>();
        _timerManager->setChronoInterval(std::chrono::milliseconds(500));
        _timerEventRegistration = getManager()->registerEventHandler<TimerEvent>(getServiceId(), this, _timerManager->getServiceId());
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
        if(evt->originatingService != _timerManager->getServiceId()) {
            LOG_ERROR(_logger, "Received event for timer which we did not register for");
            return false; // let someone else deal with it
        }

        _timerTriggerCount++;
        LOG_INFO(_logger, "Timer {} triggered {} times", _timerManager->getServiceId(), _timerTriggerCount);
        if(_timerTriggerCount == 5) {
            getManager()->pushEventThreadUnsafe<QuitEvent>(getServiceId());
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