#pragma once

#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/ConditionVariableAny.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/Common.h>
#include <atomic>
#include <queue>

namespace Ichor {
    class DependencyManager;

    // Most of the time, insertion order is not important, priority is. Not guaranteeing insertion order improves performance and somehow reduces the amount of memory used in benchmarks.
    struct PriorityQueueCompare final {
        bool operator()( const std::unique_ptr<Event>& lhs, const std::unique_ptr<Event>& rhs ) const {
            if(!lhs || !rhs) {
                return false;
            }

            return lhs->priority > rhs->priority;
        }
    };

    // When determinism is necessary, use this.
    struct OrderedPriorityQueueCompare final {
        bool operator()( const std::unique_ptr<Event>& lhs, const std::unique_ptr<Event>& rhs ) const {
            if(!lhs || !rhs) {
                return false;
            }

            if(lhs->priority == rhs->priority) {
                return lhs->id > rhs->id;
            }

            return lhs->priority > rhs->priority;
        }
    };

    template <typename COMPARE = PriorityQueueCompare>
    class TemplatePriorityQueue final : public IEventQueue {
    public:
        /// Construct a multimap based queue, supporting priorities
        /// \param spinlock Spinlock 10ms before going to sleep, improves latency in high workload cases at the expense of CPU usage
        TemplatePriorityQueue();
        explicit TemplatePriorityQueue(bool spinlock);
        ~TemplatePriorityQueue() final;

        void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) final;

        [[nodiscard]] bool empty() const noexcept final;
        [[nodiscard]] uint64_t size() const noexcept final;
        [[nodiscard]] bool is_running() const noexcept final;

        void start(bool captureSigInt) final;
        [[nodiscard]] bool shouldQuit() final;
        void quit() final;

    private:
        void shouldAddQuitEvent();

        std::priority_queue<std::unique_ptr<Event>, std::vector<std::unique_ptr<Event>>, COMPARE> _eventQueue{};
        mutable Ichor::RealtimeReadWriteMutex _eventQueueMutex{};
        ConditionVariableAny<RealtimeReadWriteMutex> _wakeup{};
        std::atomic<bool> _quit{false};
        bool _quitEventSent{false};
        bool _spinlock{false};
        std::chrono::steady_clock::time_point _whenQuitEventWasSent{};
    };

    using PriorityQueue = TemplatePriorityQueue<PriorityQueueCompare>;
    using OrderedPriorityQueue = TemplatePriorityQueue<OrderedPriorityQueueCompare>;
}
