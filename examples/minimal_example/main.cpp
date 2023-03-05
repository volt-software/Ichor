#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/timer/TimerService.h>
#include <csignal>

using namespace Ichor;

std::atomic<bool> quit{};

void siginthandler(int param) {
    quit = true;
}

class SigIntService final : public AdvancedService<SigIntService> {
public:
    SigIntService() = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        // Setup a timer that fires every 100 milliseconds
        auto timer = GetThreadLocalManager().createServiceManager<Timer, ITimer>();
        timer->setChronoInterval(100ms);

        timer->setCallback(this, [this](DependencyManager &dm) {
            // If sigint has been fired, send a quit to the event loop.
            // This can't be done from within the handler itself, as the mutex surrounding pushEvent might already be locked, resulting in a deadlock!
            if(quit) {
                dm.getEventQueue().pushEvent<QuitEvent>(getServiceId());
            }
        });
        timer->startTimer();

        // Register sigint handler
        ::signal(SIGINT, siginthandler);
        co_return {};
    }

    Task<void> stop() final {
        co_return;
    }
};

int main(int argc, char *argv[]) {
    std::locale::global(std::locale("en_US.UTF-8")); // some loggers require having a locale

    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();

    dm.createServiceManager<SigIntService>();

    // Start manager, consumes current thread.
    queue->start(DoNotCaptureSigInt);

    return 0;
}
