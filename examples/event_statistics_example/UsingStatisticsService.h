#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>

using namespace Ichor;

class UsingStatisticsService final : public AdvancedService<UsingStatisticsService> {
public:
    UsingStatisticsService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~UsingStatisticsService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "UsingStatisticsService started");
        auto quitTimerManager = GetThreadLocalManager().createServiceManager<Timer, ITimer>();
        auto bogusTimerManager = GetThreadLocalManager().createServiceManager<Timer, ITimer>();
        quitTimerManager->setChronoInterval(15s);
        bogusTimerManager->setChronoInterval(100ms);

        quitTimerManager->setCallback(this, [this](DependencyManager &dm) {
            dm.getEventQueue().pushEvent<QuitEvent>(getServiceId());
        });

        bogusTimerManager->setCallback(this, [this](DependencyManager &dm) {
            std::this_thread::sleep_for(std::chrono::milliseconds(_dist(_mt)));
        });

        quitTimerManager->startTimer();
        bogusTimerManager->startTimer();
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "UsingStatisticsService stopped");
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger&, IService&) {
        _logger = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger{nullptr};
    std::random_device _rd{};
    std::mt19937 _mt{_rd()};
    std::uniform_int_distribution<> _dist{1, 10};
};
