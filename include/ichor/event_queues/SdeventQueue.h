#pragma once

#ifndef ICHOR_USE_SDEVENT
#error "Ichor has not been compiled with sdevent support"
#endif

#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/ConditionVariableAny.h>
#include <ichor/event_queues/ISdeventQueue.h>
#include <systemd/sd-event.h>
#include <atomic>
#include <thread>

#include <map>

namespace Ichor {
    class SdeventQueue final : public ISdeventQueue {
    public:
        SdeventQueue();
        explicit SdeventQueue(uint64_t unused);
        ~SdeventQueue() final;

        void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) final;

        [[nodiscard]] bool empty() const final;
        [[nodiscard]] uint64_t size() const final;
        [[nodiscard]] bool is_running() const noexcept final;

        [[nodiscard]] sd_event* createEventLoop();
        void useEventLoop(sd_event *loop);

        bool start(bool captureSigInt) final;
        [[nodiscard]] bool shouldQuit() final;
        void quit() final;

        [[nodiscard]] NeverNull<sd_event*> getLoop() noexcept final;

    private:
        void registerEventFd();
        void registerTimer();

        mutable Ichor::RealtimeReadWriteMutex _eventQueueMutex{};
        std::multimap<uint64_t, std::unique_ptr<Event>> _otherThreadEventQueue{};
        sd_event* _eventQueue{};
        std::atomic<bool> _quit{false};
        std::atomic<bool> _initializedSdevent{false};
        int _eventfd{};
        std::thread::id _threadId{};
        sd_event_source *_eventfdSource{};
        sd_event_source *_timerSource{};
    };
}
