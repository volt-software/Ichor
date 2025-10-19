#pragma once

#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/ConditionVariableAny.h>
#include <ichor/stl/SectionalPriorityQueue.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/ConstevalHash.h>
#include <atomic>

namespace Ichor {
    class DependencyManager;

    // Most of the time, insertion order is not important, priority is. Not guaranteeing insertion order improves performance and somehow reduces the amount of memory used in benchmarks.
    struct PriorityQueueCompare final {
        [[nodiscard]] bool operator()(const std::unique_ptr<Event> &lhs, const std::unique_ptr<Event> &rhs) const noexcept {
            return lhs->priority > rhs->priority;
        }
    };

    // When determinism is necessary, use this.
    struct OrderedPriorityQueueCompare final {
        [[nodiscard]] bool operator()(const std::unique_ptr<Event>& lhs, const std::unique_ptr<Event>& rhs) const noexcept {
            if(lhs->priority == rhs->priority) {
                return lhs->id > rhs->id;
            }

            return lhs->priority > rhs->priority;
        }
    };

    template <typename COMPARE = PriorityQueueCompare>
    class TemplatePriorityQueue final : public IEventQueue {
    public:
        /// Construct a std::priority_queue based queue, supporting priorities
        /// \param spinlock Spinlock 10ms before going to sleep, improves latency in high workload cases at the expense of CPU usage
        TemplatePriorityQueue();
        explicit TemplatePriorityQueue(uint64_t quitTimeoutMs, bool spinlock = false);
        ~TemplatePriorityQueue() final;

        void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) final;

        [[nodiscard]] bool empty() const noexcept final;
        [[nodiscard]] uint64_t size() const noexcept final;
        [[nodiscard]] bool is_running() const noexcept final;
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR NameHashType get_queue_name_hash() const noexcept final;

        bool start(bool captureSigInt) final;
        [[nodiscard]] bool shouldQuit() final;
        void quit() final;

    private:
        void shouldAddQuitEvent();

        v1::SectionalPriorityQueue<std::unique_ptr<Event>, COMPARE> _eventQueue{};
        mutable v1::RealtimeReadWriteMutex _eventQueueMutex{};
        v1::ConditionVariableAny<v1::RealtimeReadWriteMutex> _wakeup{};
        std::atomic<bool> _quit{false};
        std::atomic<bool> _processingEvt{false};
        bool _quitEventSent{false};
        bool _spinlock{false};
        std::chrono::steady_clock::time_point _whenQuitEventWasSent{};
        uint64_t _quitTimeoutMs{5'000};
    };

    using PriorityQueue = TemplatePriorityQueue<PriorityQueueCompare>;
    using OrderedPriorityQueue = TemplatePriorityQueue<OrderedPriorityQueueCompare>;
}
