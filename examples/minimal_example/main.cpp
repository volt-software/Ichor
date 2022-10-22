#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>
#include <csignal>

using namespace Ichor;

std::atomic<bool> quit{};

void siginthandler(int param) {
    quit = true;
}

class SigIntService final : public Service<SigIntService> {
public:
    SigIntService() = default;

    StartBehaviour start() final {
        // Setup a timer that fires every 100 milliseconds and tell that dependency manager that we're interested in the events that the timer fires.
        auto timer = getManager()->createServiceManager<Timer, ITimer>();
        timer->setChronoInterval(100ms);

        timer->setCallback([this](TimerEvent const &evt) -> AsyncGenerator<bool> {
            // If sigint has been fired, send a quit to the event loop.
            // This can't be done from within the handler itself, as the mutex surrounding pushEvent might already be locked, resulting in a deadlock!
            if(quit) {
                getManager()->pushEvent<QuitEvent>(getServiceId());
            }
            co_return (bool)PreventOthersHandling;
        });
        timer->startTimer();

        // Register sigint handler
        ::signal(SIGINT, siginthandler);
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        return StartBehaviour::SUCCEEDED;
    }
};

int main() {
    std::locale::global(std::locale("en_US.UTF-8")); // some loggers require having a locale

    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
    // Register a framework logger and our sig int service.
    auto logger = dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
    logger->setLogLevel(LogLevel::DEBUG);
    dm.createServiceManager<SigIntService>();
    // Start manager, consumes current thread.
    queue->start(DoNotCaptureSigInt);

    return 0;
}