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
        if(_dm) [[unlikely]] {
            std::terminate();
        }

        // std::make_unique doesn't work with friends :)
        auto *dm = new DependencyManager(this);
        _dm = std::unique_ptr<DependencyManager>(dm);
        return *_dm;
    }

    void IEventQueue::startDm() {
        if(!_dm) [[unlikely]] {
            throw std::runtime_error("Please create a manager first!");
        }

        _dm->start();
    }

    void IEventQueue::processEvent(std::unique_ptr<Event> &evt) {
        _dm->processEvent(evt);
    }

    void IEventQueue::stopDm() {
        if(!_dm) [[unlikely]] {
            throw std::runtime_error("Please create a manager first!");
        }

        _dm->stop();
    }

    [[nodiscard]] IEventQueue& GetThreadLocalEventQueue() noexcept {
        return GetThreadLocalManager().getEventQueue();
    }
}
