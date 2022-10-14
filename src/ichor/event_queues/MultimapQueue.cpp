#include <ichor/event_queues/MultimapQueue.h>
#include <shared_mutex>
#include <condition_variable>

extern std::atomic<bool> sigintQuit;

namespace Ichor {

    void MultimapQueue::pushEvent(uint64_t priority, std::unique_ptr<Event> &&event) {
        {
            std::lock_guard const l(_eventQueueMutex);
            _eventQueue.insert({priority, std::move(event)});
        }
        _wakeup.notify_all();
    }

    std::pair<uint64_t, std::unique_ptr<Event>> MultimapQueue::popEvent(std::atomic<bool> &quit) {
        std::unique_lock l(_eventQueueMutex);
        _wakeup.wait_for(l, std::chrono::hours(1), [this, &quit]() {
            bool shouldQuit = sigintQuit.load(std::memory_order_acquire);

            if(shouldQuit) {
                quit.store(true, std::memory_order_release);
            }

            return quit.load(std::memory_order_acquire) || !_eventQueue.empty();
        });
        auto node = _eventQueue.extract(_eventQueue.begin());
        return {node.mapped().get()->priority, std::move(node.mapped())};
    }

    bool MultimapQueue::empty() const noexcept {
        std::shared_lock const l(_eventQueueMutex);
        return _eventQueue.empty();
    }

    uint64_t MultimapQueue::size() const noexcept {
        std::shared_lock const l(_eventQueueMutex);
        return _eventQueue.size();
    }

    void MultimapQueue::clear() noexcept {
        std::lock_guard const l(_eventQueueMutex);
        _eventQueue.clear();
    }
}