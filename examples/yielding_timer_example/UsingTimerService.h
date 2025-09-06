#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/dependency_management/IService.h>
#include <ichor/event_queues/IEventQueue.h>
#include <thread>

using namespace Ichor;
using namespace Ichor::v1;


struct IUsingTimerService {
};

class UsingTimerService final : public IUsingTimerService {
public:
    UsingTimerService(IService *self, IEventQueue *queue, ILogger *logger, ITimerFactory *timerFactory) : _logger(logger) {
            ICHOR_LOG_INFO(_logger, "UsingTimerService started");
            auto timer = timerFactory->createTimer();
            timer.setChronoInterval(std::chrono::milliseconds(250));
            timer.setCallbackAsync([this, self, queue, timer]() -> AsyncGenerator<IchorBehaviour> {
                ICHOR_LOG_INFO(_logger, "Timer {} starting 'long' task", self->getServiceId());

                _timerTriggerCount++;
                for(uint32_t i = 0; i < 5; i++) {
                    //simulate long task
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    ICHOR_LOG_INFO(_logger, "Timer {} completed 'long' task {} times", timer.getTimerId(), i);
                    // schedule us again later in the event loop for the next iteration.
                    co_yield {};
                }

                if(_timerTriggerCount == 2) {
                    queue->pushEvent<QuitEvent>(self->getServiceId());
                }

                ICHOR_LOG_INFO(_logger, "Timer {} completed 'long' task", self->getServiceId());
                co_return {};
            });
            timer.startTimer();
    }
    ~UsingTimerService() {
        ICHOR_LOG_INFO(_logger, "UsingTimerService stopped");
    }

private:
    ILogger *_logger{};
    uint64_t _timerTriggerCount{0};
};
