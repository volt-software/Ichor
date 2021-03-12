#include <ichor/DependencyManager.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>

using namespace Ichor;

std::atomic<bool> quit{};

void siginthandler(int param) {
    quit = true;
}

class SigIntService final : public Service<SigIntService> {
public:
    bool start() final {
        // Setup a timer that fires every 10 milliseconds and tell that dependency manager that we're interested in the events that the timer fires.
        auto timer = getManager()->createServiceManager<Timer, ITimer>();
        timer->setChronoInterval(std::chrono::milliseconds(10));
        // Registering an event handler requires 2 pieces of information: the service id and a pointer to a service instantiation.
        // The third piece of information is an extra filter, as we're only interested in events of this specific timer.
        _timerEventRegistration = getManager()->registerEventHandler<TimerEvent>(this, timer->getServiceId());
        timer->startTimer();

        // Register sigint handler
        signal(SIGINT, siginthandler);
        return true;
    }

    bool stop() final {
        _timerEventRegistration = nullptr;
        return true;
    }

    Generator<bool> handleEvent(TimerEvent const * const evt) {
        // If sigint has been fired, send a quit to the event loop.
        // This can't be done from within the handler itself, as the mutex surrounding pushEvent might already be locked, resulting in a deadlock!
        if(quit) {
            getManager()->pushEvent<QuitEvent>(getServiceId());
        }
        co_return (bool)PreventOthersHandling;
    }

    std::unique_ptr<EventHandlerRegistration, Deleter> _timerEventRegistration{nullptr};
};

int main() {
    std::locale::global(std::locale("en_US.UTF-8")); // some loggers require having a locale

    DependencyManager dm{};
    // Register a framework logger and our sig int service.
    auto logger = dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
    logger->setLogLevel(LogLevel::DEBUG);
    dm.createServiceManager<SigIntService>();
    // Start manager, consumes current thread.
    dm.start();

    return 0;
}