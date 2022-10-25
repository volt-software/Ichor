#include <ichor/event_queues/IEventQueue.h>
#include <ichor/DependencyManager.h>
#include <atomic>

namespace Ichor::Detail {
    std::atomic<bool> sigintQuit{false};
    std::atomic<bool> registeredSignalHandler{false};

    void on_sigint([[maybe_unused]] int sig) {
        Ichor::Detail::sigintQuit.store(true, std::memory_order_release);
    }
}

namespace Ichor {
    IEventQueue::~IEventQueue() {
        _dm = nullptr;
    }

    DependencyManager &IEventQueue::createManager() {
        // std::make_unique doesn't work with friends :)
        auto *dm = new DependencyManager(this);
        _dm = std::unique_ptr<DependencyManager>(dm);
        return *_dm;
    }

    void IEventQueue::startDm() {
        _dm->start();
    }

    void IEventQueue::processEvent(std::unique_ptr<Event> &&evt) {
        _dm->processEvent(std::move(evt));
    }

    void IEventQueue::stopDm() {
        _dm->stop();
    }
}