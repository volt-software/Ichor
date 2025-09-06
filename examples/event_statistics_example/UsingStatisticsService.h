#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/event_queues/IEventQueue.h>
#include <thread>

using namespace Ichor;
using namespace Ichor::v1;

class UsingStatisticsService final {
public:
    UsingStatisticsService(IService *self, IEventQueue *queue, ILogger *logger, ITimerFactory *timerFactory) : _logger(logger) {
        ICHOR_LOG_INFO(_logger, "UsingStatisticsService started");
        auto quitTimer = timerFactory->createTimer();
        auto bogusTimer = timerFactory->createTimer();
        quitTimer.setChronoInterval(1s);
        bogusTimer.setChronoInterval(50ms);

        quitTimer.setCallback([self, queue]() {
            queue->pushEvent<QuitEvent>(self->getServiceId());
        });

        bogusTimer.setCallback([this]() {
            ICHOR_LOG_INFO(_logger, "bogus timer callback");
            std::this_thread::sleep_for(std::chrono::milliseconds(_dist(_mt)));
        });

        quitTimer.startTimer();
        bogusTimer.startTimer();
    }
    ~UsingStatisticsService() {
        ICHOR_LOG_INFO(_logger, "UsingStatisticsService stopped");
    }

private:
    ILogger *_logger;
    std::random_device _rd{};
    std::mt19937 _mt{_rd()};
    std::uniform_int_distribution<> _dist{1, 10};
};
