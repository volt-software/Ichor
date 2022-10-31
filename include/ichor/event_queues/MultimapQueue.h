#pragma once

#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/ConditionVariableAny.h>
#include <ichor/event_queues/IEventQueue.h>
#include <atomic>

#ifdef ICHOR_USE_ABSEIL
#include <absl/container/btree_map.h>
#else
#include <map>
#endif

namespace Ichor {
    class DependencyManager;

    class MultimapQueue final : public IEventQueue {
    public:
        ~MultimapQueue() final;

        void pushEvent(uint64_t priority, std::unique_ptr<Event> &&event) final;

        [[nodiscard]] bool empty() const noexcept final;
        [[nodiscard]] uint64_t size() const noexcept final;

        void start(bool captureSigInt) final;
        [[nodiscard]] bool shouldQuit() final;
        void quit() final;

    private:
        void shouldAddQuitEvent();

#ifdef ICHOR_USE_ABSEIL
        absl::btree_multimap<uint64_t, std::unique_ptr<Event>> _eventQueue{};
#else
        std::multimap<uint64_t, std::unique_ptr<Event>> _eventQueue{};
#endif
        mutable Ichor::RealtimeReadWriteMutex _eventQueueMutex{};
        ConditionVariableAny<RealtimeReadWriteMutex> _wakeup{};
        std::atomic<bool> _quit{false};
        bool _quitEventSent{false};
        std::chrono::steady_clock::time_point _whenQuitEventWasSent{};
    };
}