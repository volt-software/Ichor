#pragma once

#include <map>
#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/ConditionVariableAny.h>
#include <ichor/event_queues/IEventQueue.h>
#include <atomic>

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
        std::multimap<uint64_t, std::unique_ptr<Event>> _eventQueue{};
        mutable Ichor::RealtimeReadWriteMutex _eventQueueMutex{};
        ConditionVariableAny<RealtimeReadWriteMutex> _wakeup{};
        std::atomic<bool> _quit{false};
    };
}