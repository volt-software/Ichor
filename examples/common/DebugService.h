#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include "TestMsg.h"

using namespace Ichor;

class DebugService final : public AdvancedService<DebugService> {
public:
    DebugService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED, Properties{{"LogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_INFO)}});
        reg.registerDependency<ITimerFactory>(this, DependencyFlags::REQUIRED);
    }
    ~DebugService() final = default;
private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        auto &_timer = _timerFactory->createTimer();
        _timer.setCallback([this]() {
            auto svcs = dm.getAllServices();
            for(auto &[id, svc] : svcs) {
                ICHOR_LOG_INFO(_logger, "Svc {}:{} {}", svc->getServiceId(), svc->getServiceName(), svc->getServiceState());
            }
        });
        _timer.setChronoInterval(1s);
        _timer.startTimer();

        co_return {};
    }

    Task<void> stop() final {
        // the timer factory also stops all timers registered to this service
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger &logger, IService&) {
        _logger = nullptr;
    }

    void addDependencyInstance(ITimerFactory &factory, IService &) {
        _timerFactory = &factory;
    }

    void removeDependencyInstance(ITimerFactory &factory, IService&) {
        _timerFactory = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger{};
    ITimerFactory *_timerFactory{};

};
