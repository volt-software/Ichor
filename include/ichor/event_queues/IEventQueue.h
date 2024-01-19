#pragma once

#include <cstdint>
#include <memory>
#include <atomic>
#include <ichor/events/Event.h>
#include <ichor/Concepts.h>

namespace Ichor {
    class DependencyManager;

    class IEventQueue {
    public:
        virtual ~IEventQueue();

        [[nodiscard]] virtual bool empty() const = 0;
        [[nodiscard]] virtual uint64_t size() const = 0;
        [[nodiscard]] virtual bool is_running() const noexcept = 0;

        /// Starts the event loop, consumes the current thread until a QuitEvent occurs
        /// \param captureSigInt If true, exit on CTRL+C/SigInt
        virtual void start(bool captureSigInt) = 0;

        /// Create manager associated with this queue. Terminates the program if called twice.
        /// \return
        DependencyManager& createManager();

        /// Thread-safe. Push event into event loop with the default priority (1000)
        /// \tparam EventT Type of event to push, has to derive from Event
        /// \tparam Args auto-deducible arguments for EventT constructor
        /// \param originatingServiceId service that is pushing the event
        /// \param args arguments for EventT constructor
        /// \return event id (can be used in completion/error handlers)
        template <typename EventT, typename... Args>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<EventT, Event>
#endif
        uint64_t pushEvent(uint64_t originatingServiceId, Args&&... args) {
            static_assert(EventT::TYPE == typeNameHash<EventT>(), "Event typeNameHash wrong");
            static_assert(EventT::NAME == typeName<EventT>(), "Event typeName wrong");

            uint64_t eventId = getNextEventId();
            pushEventInternal(INTERNAL_EVENT_PRIORITY, std::unique_ptr<Event>{new EventT(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), INTERNAL_EVENT_PRIORITY, std::forward<Args>(args)...)});
//            ICHOR_LOG_TRACE(_logger, "inserted event of type {} into manager {}", typeName<EventT>(), _dm->getId());
            return eventId;
        }

        /// Thread-safe. Push event into event loop with specified priority
        /// \tparam EventT Type of event to push, has to derive from Event
        /// \tparam Args auto-deducible arguments for EventT constructor
        /// \param originatingServiceId service that is pushing the event
        /// \param args arguments for EventT constructor
        /// \return event id (can be used in completion/error handlers)
        template <typename EventT, typename... Args>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<EventT, Event>
#endif
        uint64_t pushPrioritisedEvent(uint64_t originatingServiceId, uint64_t priority, Args&&... args) {
            static_assert(EventT::TYPE == typeNameHash<EventT>(), "Event typeNameHash wrong");
            static_assert(EventT::NAME == typeName<EventT>(), "Event typeName wrong");

            uint64_t eventId = getNextEventId();
            pushEventInternal(priority, std::unique_ptr<Event>{new EventT(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), std::forward<uint64_t>(priority), std::forward<Args>(args)...)});
//            ICHOR_LOG_TRACE(_logger, "inserted event of type {} into manager {}", typeName<EventT>(), _dm->getId());
            return eventId;
        }

        /// Thread-safe. Get the next event ID for this queue (not a global counter)
        /// \return next event id
        [[nodiscard]] uint64_t getNextEventId() noexcept {
            return _eventIdCounter.fetch_add(1, std::memory_order_relaxed);
        }

    protected:
        friend class DependencyManager;
        [[nodiscard]] virtual bool shouldQuit() = 0;
        virtual void quit() = 0;
        virtual void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) = 0;
        void startDm();
        void processEvent(std::unique_ptr<Event> &evt);
        void stopDm();

        std::unique_ptr<DependencyManager> _dm;
        std::atomic<uint64_t> _eventIdCounter{0};
    };

    /// Get event queue associated with current thread. Terminates program if none available.
    /// \return Event queue
    [[nodiscard]] IEventQueue& GetThreadLocalEventQueue() noexcept;

    //
    inline constexpr bool DoNotCaptureSigInt = false;
    inline constexpr bool CaptureSigInt = true;
}
