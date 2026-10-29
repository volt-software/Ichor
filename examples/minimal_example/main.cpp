#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/DependencyManager.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/ichor-mimalloc.h>
#include <csignal>

using namespace Ichor;
using namespace Ichor::v1;

std::atomic<bool> quit{};

void siginthandler(int) {
    quit = true;
}

class SigIntService final {
public:
    explicit SigIntService(ScopedServiceProxy<ITimerFactory> factory) : _timer(factory->createTimer()) {
        // Setup a timer that fires every 100 milliseconds
        _timer.setChronoInterval(100ms);

        _timer.setCallback([]() {
            // If sigint has been fired, send a quit to the event loop.
            // This can't be done from within the siginthandler itself, as the mutex surrounding pushEvent might already be locked, resulting in a deadlock!
            if(quit) {
                GetThreadLocalEventQueue().pushEvent<QuitEvent>(ServiceIdType{});
            }
        });
        _timer.startTimer();

        // Register sigint handler
        auto r = ::signal(SIGINT, siginthandler);
        if(r == SIG_ERR) {
            std::terminate();
        }
    }

private:
    // The timer is stopped and destroyed together with the last TimerRef to it
    TimerRef _timer;
};

int main(int argc, char *argv[]) {
    // some loggers require having a locale
#if ICHOR_EXCEPTIONS_ENABLED
    try {
#endif
        std::locale::global(std::locale("en_US.UTF-8"));
#if ICHOR_EXCEPTIONS_ENABLED
    } catch(std::runtime_error const &e) {
        fmt::println("Couldn't set locale to en_US.UTF-8: {}", e.what());
    }
#endif

    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->getManager();

    dm.createServiceManager<SigIntService>();
    dm.createServiceManager<TimerFactoryFactory>();

    // Start manager, consumes current thread.
    queue->start(DoNotCaptureSigInt);

    return 0;
}
