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
        virtual void start(bool captureSigInt) = 0;
        DependencyManager& createManager();



        /// Push event into event loop with the default priority
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

            // TODO hardening
//#ifdef ICHOR_USE_HARDENING
//            if(originatingServiceId != 0 && _services.find(originatingServiceId) == _services.end()) [[unlikely]] {
//                std::terminate();
//            }
//#endif

            uint64_t eventId = getNextEventId();
            pushEventInternal(INTERNAL_EVENT_PRIORITY, std::unique_ptr<Event>{new EventT(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), INTERNAL_EVENT_PRIORITY, std::forward<Args>(args)...)});
//            ICHOR_LOG_TRACE(_logger, "inserted event of type {} into manager {}", typeName<EventT>(), getId());
            return eventId;
        }

        /// Push event into event loop with specified priority
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

            // TODO hardening
//#ifdef ICHOR_USE_HARDENING
//            if(originatingServiceId != 0 && _services.find(originatingServiceId) == _services.end()) [[unlikely]] {
//                std::terminate();
//            }
//#endif

            uint64_t eventId = getNextEventId();
            pushEventInternal(priority, std::unique_ptr<Event>{new EventT(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), std::forward<uint64_t>(priority), std::forward<Args>(args)...)});
//            ICHOR_LOG_TRACE(_logger, "inserted event of type {} into manager {}", typeName<EventT>(), dm->getId());
            return eventId;
        }

        virtual void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) = 0;

        [[nodiscard]] uint64_t getNextEventId() noexcept {
            return _eventIdCounter.fetch_add(1, std::memory_order_relaxed);
        }

    protected:
        friend class DependencyManager;
        [[nodiscard]] virtual bool shouldQuit() = 0;
        virtual void quit() = 0;
        void startDm();
        void processEvent(std::unique_ptr<Event> &&evt);
        void stopDm();

        std::unique_ptr<DependencyManager> _dm;
        std::atomic<uint64_t> _eventIdCounter{0};
    };

    [[nodiscard]] IEventQueue& GetThreadLocalEventQueue() noexcept;

    inline constexpr bool DoNotCaptureSigInt = false;
    inline constexpr bool CaptureSigInt = true;
}
