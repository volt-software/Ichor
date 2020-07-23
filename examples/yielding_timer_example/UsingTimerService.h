#pragma once

#include <coroutine>
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

    cppcoro::generator<EventCallbackReturn> handleEvent(TimerEvent const * const evt) {
        if(evt->originatingService != _timerManager->getServiceId()) {
            LOG_ERROR(_logger, "Received event for timer which we did not register for");
            co_yield EventCallbackReturn{false, true}; // let someone else deal with it
        }

        LOG_INFO(_logger, "Timer {} starting 'long' task", getServiceId());

        _timerTriggerCount++;
        for(uint32_t i = 0; i < 5; i++) {
            //simulate long task
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            LOG_INFO(_logger, "Timer {} completed 'long' task {} times", getServiceId(), i);
            // schedule us again later in the event loop for the next iteration, don't let other handlers handle this event.
            co_yield EventCallbackReturn{true, false};
        }

        if(_timerTriggerCount == 2) {
            getManager()->pushEventThreadUnsafe<QuitEvent>(getServiceId());
        }

        LOG_INFO(_logger, "Timer {} completed 'long' task", getServiceId());
        // we dealt with it, don't propagate to other handlers
        co_yield EventCallbackReturn{false, false};
    }

private:
    ILogger *_logger{nullptr};
    std::unique_ptr<EventHandlerRegistration> _timerEventRegistration{nullptr};
    uint64_t _timerTriggerCount{0};
    Timer* _timerManager{nullptr};
};