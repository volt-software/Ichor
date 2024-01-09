#pragma once

#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/ConditionVariableAny.h>
#include <ichor/event_queues/IEventQueue.h>
#include <atomic>

#include <map>

namespace Ichor {
    class DependencyManager;

    class MultimapQueue final : public IEventQueue {
    public:
        /// Construct a multimap based queue, supporting priorities
        /// \param spinlock Spinlock 10ms before going to sleep, improves latency in high workload cases at the expense of CPU usage
        MultimapQueue();
        explicit MultimapQueue(bool spinlock);
        ~MultimapQueue() final;

        void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) final;

        [[nodiscard]] bool empty() const noexcept final;
        [[nodiscard]] uint64_t size() const noexcept final;
        [[nodiscard]] bool is_running() const noexcept final;

        void start(bool captureSigInt) final;
        [[nodiscard]] bool shouldQuit() final;
        void quit() final;

    private:
        void shouldAddQuitEvent();

        std::multimap<uint64_t, std::unique_ptr<Event>> _eventQueue{};
        mutable Ichor::RealtimeReadWriteMutex _eventQueueMutex{};
        ConditionVariableAny<RealtimeReadWriteMutex> _wakeup{};
        std::atomic<bool> _quit{false};
        bool _quitEventSent{false};
        bool _spinlock{false};
        std::chrono::steady_clock::time_point _whenQuitEventWasSent{};
    };
}
