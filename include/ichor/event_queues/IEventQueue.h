#pragma once

#include <cstdint>
#include <memory>
#include <ichor/events/Event.h>

namespace Ichor {
    class DependencyManager;

    class IEventQueue {
    public:
        virtual ~IEventQueue();

        virtual void pushEvent(uint64_t priority, std::unique_ptr<Event> &&event) = 0;

        [[nodiscard]] virtual bool empty() const = 0;
        [[nodiscard]] virtual uint64_t size() const = 0;
        virtual void start(bool captureSigInt) = 0;
        DependencyManager& createManager();

    protected:
        friend class DependencyManager;
        [[nodiscard]] virtual bool shouldQuit() = 0;
        virtual void quit() = 0;
        void startDm();
        void processEvent(Event *evt);
        void stopDm();

        std::unique_ptr<DependencyManager> _dm;
    };

    inline constexpr bool DoNotCaptureSigInt = false;
    inline constexpr bool CaptureSigInt = true;
}