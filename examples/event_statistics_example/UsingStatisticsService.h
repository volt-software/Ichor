#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/Service.h>
#include "ichor/dependency_management/ILifecycleManager.h"

using namespace Ichor;

class UsingStatisticsService final : public Service<UsingStatisticsService> {
public:
    UsingStatisticsService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~UsingStatisticsService() final = default;

private:
    AsyncGenerator<void> start() final {
        ICHOR_LOG_INFO(_logger, "UsingStatisticsService started");
        auto quitTimerManager = getManager().createServiceManager<Timer, ITimer>();
        auto bogusTimerManager = getManager().createServiceManager<Timer, ITimer>();
        quitTimerManager->setChronoInterval(15s);
        bogusTimerManager->setChronoInterval(100ms);

        quitTimerManager->setCallback(this, [this](DependencyManager &dm) {
            getManager().pushEvent<QuitEvent>(getServiceId());
        });

        bogusTimerManager->setCallback(this, [this](DependencyManager &dm) {
            std::this_thread::sleep_for(std::chrono::milliseconds(_dist(_mt)));
        });

        quitTimerManager->startTimer();
        bogusTimerManager->startTimer();
        co_return;
    }

    AsyncGenerator<void> stop() final {
        ICHOR_LOG_INFO(_logger, "UsingStatisticsService stopped");
        co_return;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{nullptr};
    std::random_device _rd{};
    std::mt19937 _mt{_rd()};
    std::uniform_int_distribution<> _dist{1, 10};
};
