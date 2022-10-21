#pragma once

#ifdef ICHOR_USE_SDEVENT

#include <map>
#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/ConditionVariableAny.h>
#include <ichor/event_queues/IEventQueue.h>
#include <systemd/sd-event.h>
#include <atomic>
#include <thread>

namespace Ichor {
    class SdeventQueue final : public IEventQueue {
    public:
        SdeventQueue();
        ~SdeventQueue() final;

        void pushEvent(uint64_t priority, std::unique_ptr<Event> &&event) final;

        [[nodiscard]] bool empty() const final;
        [[nodiscard]] uint64_t size() const final;

        [[nodiscard]] sd_event* createEventLoop();
        void useEventLoop(sd_event *loop);

        void start(bool captureSigInt) final;
        [[nodiscard]] bool shouldQuit() final;
        void quit() final;

    private:
        void registerEventFd();

        mutable Ichor::RealtimeReadWriteMutex _eventQueueMutex{};
        sd_event* _eventQueue{};
        std::atomic<bool> _quit{false};
        std::atomic<bool> _initializedSdevent{false};
        int _eventfd{};
        std::thread::id _threadId{};
        sd_event_source *_eventfd_source{nullptr};
    };
}

#endif
