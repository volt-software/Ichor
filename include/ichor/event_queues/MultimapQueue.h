#pragma once

#include <map>
#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/ConditionVariableAny.h>
#include <ichor/event_queues/IEventQueue.h>

namespace Ichor {
    class MultimapQueue final : public IEventQueue {
    public:
        ~MultimapQueue() final = default;

        void pushEvent(uint64_t priority, std::unique_ptr<Event> &&event) final;
        std::pair<uint64_t, std::unique_ptr<Event>> popEvent(std::atomic<bool> &quit) final;

        bool empty() const noexcept final;
        uint64_t size() const noexcept final;
        void clear() noexcept final;

    private:
        std::multimap<uint64_t, std::unique_ptr<Event>> _eventQueue{};
        mutable RealtimeReadWriteMutex _eventQueueMutex{};
        ConditionVariableAny<RealtimeReadWriteMutex> _wakeup{};
    };
}