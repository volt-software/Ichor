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

        timer->setCallback([this](TimerEvent const * const evt) -> Generator<bool> {
            // If sigint has been fired, send a quit to the event loop.
            // This can't be done from within the handler itself, as the mutex surrounding pushEvent might already be locked, resulting in a deadlock!
            if(quit) {
                getManager()->pushEvent<QuitEvent>(getServiceId());
            }
            co_return (bool)PreventOthersHandling;
        });
        timer->startTimer();

        // Register sigint handler
        signal(SIGINT, siginthandler);
        return true;
    }

    bool stop() final {
        return true;
    }
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