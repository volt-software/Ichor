#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>

using namespace Ichor;

class UsingTimerService final : public AdvancedService<UsingTimerService> {
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
        _timer->setChronoInterval(std::chrono::milliseconds(50));
        _timer->setCallback([this]() {
            handleEvent();
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

    void removeDependencyInstance(ITimerFactory &factory, IService&) {
        _timerFactory = nullptr;
    }

    void handleEvent() {
        _timerTriggerCount++;
        ICHOR_LOG_INFO(_logger, "Timer {} triggered {} times", _timer->getTimerId(), _timerTriggerCount);
        if(_timerTriggerCount == 5) {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        }
    }

    friend DependencyRegister;

    ILogger *_logger{nullptr};
    uint64_t _timerTriggerCount{0};
    ITimerFactory *_timerFactory{nullptr};
    ITimer *_timer{nullptr};
};
