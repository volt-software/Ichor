#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;

class UsingStatisticsService final : public Service<UsingStatisticsService> {
public:
    UsingStatisticsService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~UsingStatisticsService() final = default;

    StartBehaviour start() final {
        ICHOR_LOG_INFO(_logger, "UsingStatisticsService started");
        auto quitTimerManager = getManager().createServiceManager<Timer, ITimer>();
        auto bogusTimerManager = getManager().createServiceManager<Timer, ITimer>();
        quitTimerManager->setChronoInterval(15s);
        bogusTimerManager->setChronoInterval(100ms);

        quitTimerManager->setCallback(this, [this](DependencyManager &dm) -> AsyncGenerator<void> {
            getManager().pushEvent<QuitEvent>(getServiceId());
            co_return;
        });

        bogusTimerManager->setCallback(this, [this](DependencyManager &dm) -> AsyncGenerator<void> {
            std::this_thread::sleep_for(std::chrono::milliseconds(_dist(_mt)));
            co_return;
        });

        quitTimerManager->startTimer();
        bogusTimerManager->startTimer();
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        ICHOR_LOG_INFO(_logger, "UsingStatisticsService stopped");
        return StartBehaviour::SUCCEEDED;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

private:
    ILogger *_logger{nullptr};
    std::random_device _rd{};
    std::mt19937 _mt{_rd()};
    std::uniform_int_distribution<> _dist{1, 10};
};