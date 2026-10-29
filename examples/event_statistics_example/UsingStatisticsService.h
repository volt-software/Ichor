#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/event_queues/IEventQueue.h>
#include <thread>

using namespace Ichor;
using namespace Ichor::v1;

class UsingStatisticsService final {
public:
    UsingStatisticsService(IService *self, IEventQueue *queue, ScopedServiceProxy<ILogger> logger, ScopedServiceProxy<ITimerFactory> timerFactory) : _logger(logger), _quitTimer(timerFactory->createTimer()), _bogusTimer(timerFactory->createTimer()) {
        ICHOR_LOG_INFO(_logger, "UsingStatisticsService started");
        _quitTimer.setChronoInterval(1s);
        _bogusTimer.setChronoInterval(50ms);

        _quitTimer.setCallback([self, queue]() {
            queue->pushEvent<QuitEvent>(self->getServiceId());
        });

        _bogusTimer.setCallback([this]() {
            ICHOR_LOG_INFO(_logger, "bogus timer callback");
            std::this_thread::sleep_for(std::chrono::milliseconds(_dist(_mt)));
        });

        _quitTimer.startTimer();
        _bogusTimer.startTimer();
    }
    ~UsingStatisticsService() {
        ICHOR_LOG_INFO(_logger, "UsingStatisticsService stopped");
    }

private:
    ScopedServiceProxy<ILogger> _logger;
    TimerRef _quitTimer;
    TimerRef _bogusTimer;
    std::random_device _rd{};
    std::mt19937 _mt{_rd()};
    std::uniform_int_distribution<> _dist{1, 10};
};
