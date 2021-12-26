#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>
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
        auto quitTimerManager = getManager()->createServiceManager<Timer, ITimer>();
        auto bogusTimerManager = getManager()->createServiceManager<Timer, ITimer>();
        quitTimerManager->setChronoInterval(std::chrono::seconds (15));
        bogusTimerManager->setChronoInterval(std::chrono::milliseconds (15));

        quitTimerManager->setCallback([this](TimerEvent const * const) -> Generator<bool> {
            getManager()->pushEvent<QuitEvent>(getServiceId(), INTERNAL_EVENT_PRIORITY + 1);
            co_return (bool)PreventOthersHandling;
        });

        bogusTimerManager->setCallback([this](TimerEvent const * const) -> Generator<bool> {
            std::this_thread::sleep_for(std::chrono::milliseconds(_dist(_mt)));
            co_return (bool)PreventOthersHandling;
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