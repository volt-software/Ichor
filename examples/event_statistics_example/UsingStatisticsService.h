#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <thread>

using namespace Ichor;

class UsingStatisticsService final : public AdvancedService<UsingStatisticsService> {
public:
    UsingStatisticsService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ITimerFactory>(this, true);
    }
    ~UsingStatisticsService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "UsingStatisticsService started");
        auto &quitTimer = _timerFactory->createTimer();
        auto &bogusTimer = _timerFactory->createTimer();
        quitTimer.setChronoInterval(5s);
        bogusTimer.setChronoInterval(100ms);

        quitTimer.setCallback([this]() {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        });

        bogusTimer.setCallback([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(_dist(_mt)));
        });

        quitTimer.startTimer();
        bogusTimer.startTimer();
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

    void addDependencyInstance(ITimerFactory &factory, IService &) {
        _timerFactory = &factory;
    }

    void removeDependencyInstance(ITimerFactory &factory, IService&) {
        _timerFactory = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger{nullptr};
    ITimerFactory *_timerFactory{nullptr};
    std::random_device _rd{};
    std::mt19937 _mt{_rd()};
    std::uniform_int_distribution<> _dist{1, 10};
};
