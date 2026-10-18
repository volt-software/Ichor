#include <ichor/event_queues/IEventQueue.h>
#include <ichor/DependencyManager.h>
#include <atomic>

namespace Ichor::v1::Detail {
    std::atomic<bool> sigintQuit{false};
    std::atomic<bool> registeredSignalHandler{false};

    void on_sigint([[maybe_unused]] int sig) {
        Ichor::v1::Detail::sigintQuit.store(true, std::memory_order_release);
    }
}

namespace Ichor {
    [[nodiscard]] IEventQueue& GetThreadLocalEventQueue() noexcept {
        return GetThreadLocalManager().getEventQueue();
    }
}
