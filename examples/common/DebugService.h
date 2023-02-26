#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include "TestMsg.h"

using namespace Ichor;

class DebugService final : public AdvancedService<DebugService> {
public:
    DebugService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true, Properties{{"LogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_INFO)}});
    }
    ~DebugService() final = default;
private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        _timer = GetThreadLocalManager().createServiceManager<Timer, ITimer>();
        _timer->setCallback(this, [this](DependencyManager &dm) {
            auto svcs = dm.getServiceInfo();
            for(auto &[id, svc] : svcs) {
                ICHOR_LOG_INFO(_logger, "Svc {}:{} {}", svc->getServiceId(), svc->getServiceName(), svc->getServiceState());
            }
        });
        _timer->setChronoInterval(1s);
        _timer->startTimer();

        co_return {};
    }

    Task<void> stop() final {
        _timer->stopTimer();

        co_return;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger{nullptr};
    Timer *_timer{nullptr};

};
