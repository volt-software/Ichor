#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;


struct IUsingTimerService {
};

class UsingTimerService final : public IUsingTimerService, public Service<UsingTimerService> {
public:
    UsingTimerService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~UsingTimerService() final = default;

private:
    StartBehaviour start() final {
        ICHOR_LOG_INFO(_logger, "UsingTimerService started");
        _timerManager = getManager().createServiceManager<Timer, ITimer>();
        _timerManager->setChronoInterval(std::chrono::milliseconds(250));
        _timerManager->setCallback(this, [this](DependencyManager &dm) {
            return handleEvent(dm);
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

    AsyncGenerator<void> handleEvent(DependencyManager &dm) {
        ICHOR_LOG_INFO(_logger, "Timer {} starting 'long' task", getServiceId());

        _timerTriggerCount++;
        for(uint32_t i = 0; i < 5; i++) {
            //simulate long task
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ICHOR_LOG_INFO(_logger, "Timer {} completed 'long' task {} times", getServiceId(), i);
            // schedule us again later in the event loop for the next iteration.
            co_yield empty;
        }

        if(_timerTriggerCount == 2) {
            getManager().pushEvent<QuitEvent>(getServiceId());
        }

        ICHOR_LOG_INFO(_logger, "Timer {} completed 'long' task", getServiceId());
        co_return;
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{nullptr};
    uint64_t _timerTriggerCount{0};
    Timer* _timerManager{nullptr};
};