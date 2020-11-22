#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;

class UsingStatisticsService final : public Service {
public:
    UsingStatisticsService(DependencyRegister &reg, IchorProperties props) : Service(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~UsingStatisticsService() final = default;

    bool start() final {
        LOG_INFO(_logger, "UsingStatisticsService started");
        auto _quitTimerManager = getManager()->createServiceManager<Timer, ITimer>();
        auto _bogusTimerManager = getManager()->createServiceManager<Timer, ITimer>();
        _quitTimerManager->setChronoInterval(std::chrono::seconds (15));
        _bogusTimerManager->setChronoInterval(std::chrono::milliseconds (15));
        _quitTimerId = _quitTimerManager->getServiceId();
        _quitTimerEventRegistration = getManager()->registerEventHandler<TimerEvent>(getServiceId(), this, _quitTimerManager->getServiceId());
        _bogusTimerEventRegistration = getManager()->registerEventHandler<TimerEvent>(getServiceId(), this, _bogusTimerManager->getServiceId());
        _quitTimerManager->startTimer();
        _bogusTimerManager->startTimer();
        return true;
    }

    bool stop() final {
        _quitTimerEventRegistration = nullptr;
        _bogusTimerEventRegistration = nullptr;
        LOG_INFO(_logger, "UsingStatisticsService stopped");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    Generator<bool> handleEvent(TimerEvent const * const evt) {
        if(evt->originatingService == _quitTimerId) {
            getManager()->pushEvent<QuitEvent>(getServiceId(), INTERNAL_EVENT_PRIORITY + 1);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(_dist(_mt)));
        }

        co_return (bool)PreventOthersHandling;
    }

private:
    ILogger *_logger{nullptr};
    std::unique_ptr<EventHandlerRegistration> _quitTimerEventRegistration{nullptr};
    std::unique_ptr<EventHandlerRegistration> _bogusTimerEventRegistration{nullptr};
    uint64_t _quitTimerId{0};

    std::random_device _rd{};
    std::mt19937 _mt{_rd()};
    std::uniform_int_distribution<> _dist{1, 10};
};