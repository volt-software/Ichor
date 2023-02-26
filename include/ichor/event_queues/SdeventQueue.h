#pragma once

#ifdef ICHOR_USE_SDEVENT

#include <map>
#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/ConditionVariableAny.h>
#include <ichor/event_queues/IEventQueue.h>
#include <systemd/sd-event.h>
#include <atomic>
#include <thread>

#ifdef ICHOR_USE_ABSEIL
#include <absl/container/btree_map.h>
#else
#include <map>
#endif

namespace Ichor {
    class SdeventQueue final : public IEventQueue {
    public:
        SdeventQueue();
        ~SdeventQueue() final;

        void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) final;

        [[nodiscard]] bool empty() const final;
        [[nodiscard]] uint64_t size() const final;

        [[nodiscard]] sd_event* createEventLoop();
        void useEventLoop(sd_event *loop);

        void start(bool captureSigInt) final;
        [[nodiscard]] bool shouldQuit() final;
        void quit() final;

    private:
        void registerEventFd();
        void registerTimer();

        mutable Ichor::RealtimeReadWriteMutex _eventQueueMutex{};
#ifdef ICHOR_USE_ABSEIL
        absl::btree_multimap<uint64_t, std::unique_ptr<Event>> _otherThreadEventQueue{};
#else
        std::multimap<uint64_t, std::unique_ptr<Event>> _otherThreadEventQueue{};
#endif
        sd_event* _eventQueue{};
        std::atomic<bool> _quit{false};
        std::atomic<bool> _initializedSdevent{false};
        int _eventfd{};
        std::thread::id _threadId{};
        sd_event_source *_eventfdSource{nullptr};
        sd_event_source *_timerSource{nullptr};
    };
}

#endif
