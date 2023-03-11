#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <thread>

using namespace Ichor;


struct IUsingTimerService {
};

class UsingTimerService final : public IUsingTimerService, public AdvancedService<UsingTimerService> {
public:
    UsingTimerService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ITimerFactory>(this, true);
    }
    ~UsingTimerService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "UsingTimerService started");
        _timer = &_timerFactory->createTimer();
        _timer->setChronoInterval(std::chrono::milliseconds(250));
        _timer->setCallbackAsync([this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
            return handleEvent(dm);
        });
        _timer->startTimer();
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "UsingTimerService stopped");
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger&, IService&) {
        _logger = nullptr;
    }

    void addDependencyInstance(ITimerFactory &factory, IService &) {
        _timerFactory = &factory;
    }

    void removeDependencyInstance(ITimerFactory &, IService&) {
        _timerFactory = nullptr;
    }

    AsyncGenerator<IchorBehaviour> handleEvent(DependencyManager &dm) {
        ICHOR_LOG_INFO(_logger, "Timer {} starting 'long' task", getServiceId());

        _timerTriggerCount++;
        for(uint32_t i = 0; i < 5; i++) {
            //simulate long task
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ICHOR_LOG_INFO(_logger, "Timer {} completed 'long' task {} times", _timer->getTimerId(), i);
            // schedule us again later in the event loop for the next iteration.
            co_yield {};
        }

        if(_timerTriggerCount == 2) {
            dm.getEventQueue().pushEvent<QuitEvent>(getServiceId());
        }

        ICHOR_LOG_INFO(_logger, "Timer {} completed 'long' task", getServiceId());
        co_return {};
    }

    friend DependencyRegister;

    ILogger *_logger{nullptr};
    uint64_t _timerTriggerCount{0};
    ITimerFactory *_timerFactory{nullptr};
    ITimer *_timer{nullptr};
};
