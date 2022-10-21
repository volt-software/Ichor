#include <ichor/event_queues/MultimapQueue.h>
#include <shared_mutex>
#include <condition_variable>
#include <ichor/DependencyManager.h>
#include <csignal>

namespace Ichor::Detail {
    extern std::atomic<bool> sigintQuit;
    extern std::atomic<bool> registeredSignalHandler;
    void on_sigint([[maybe_unused]] int sig);
}

namespace Ichor {
    MultimapQueue::~MultimapQueue() {
        stopDm();
    }

    void MultimapQueue::pushEvent(uint64_t priority, std::unique_ptr<Event> &&event) {
        if(!event) {
            throw std::runtime_error("Pushing nullptr");
        }

        {
            std::lock_guard const l(_eventQueueMutex);
            _eventQueue.insert({priority, std::move(event)});
        }
        _wakeup.notify_all();
    }

    bool MultimapQueue::empty() const noexcept {
        std::shared_lock const l(_eventQueueMutex);
        return _eventQueue.empty();
    }

    uint64_t MultimapQueue::size() const noexcept {
        std::shared_lock const l(_eventQueueMutex);
        return _eventQueue.size();
    }

    void MultimapQueue::start(bool captureSigInt) {
        if(!_dm) {
            throw std::runtime_error("Please create a manager first!");
        }

        if(captureSigInt && !Ichor::Detail::registeredSignalHandler.exchange(true)) {
            if (::signal(SIGINT, Ichor::Detail::on_sigint) == SIG_ERR) {
                throw std::runtime_error("Couldn't set signal");
            }
            Ichor::Detail::registeredSignalHandler = true;
        }

        startDm();

        while(!shouldQuit()) {
            std::unique_lock l(_eventQueueMutex);
            _wakeup.wait_for(l, std::chrono::hours(1), [this]() {
                return shouldQuit() || !_eventQueue.empty();
            });
            auto node = _eventQueue.extract(_eventQueue.begin());
            l.unlock();
            processEvent(node.mapped().get());
//            return {node.mapped().get()->priority, std::move(node.mapped())};
        }

        stopDm();
    }

    bool MultimapQueue::shouldQuit() {
        bool const shouldQuit = Detail::sigintQuit.load(std::memory_order_acquire);

        if (shouldQuit) {
            _quit.store(true, std::memory_order_release);
        }

        return _quit.load(std::memory_order_acquire);
    }

    void MultimapQueue::quit() {
        _quit.store(true, std::memory_order_release);
    }
}