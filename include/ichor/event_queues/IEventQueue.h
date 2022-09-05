#pragma once

#include <cstdint>
#include <memory>
#include <ichor/Events.h>

namespace Ichor {
    class IEventQueue {
    public:
        virtual ~IEventQueue() = default;

        virtual void pushEvent(uint64_t priority, std::unique_ptr<Event> &&event) = 0;
        virtual std::pair<uint64_t, std::unique_ptr<Event>> popEvent(std::atomic<bool> &quit) = 0;

        [[nodiscard]] virtual bool empty() const noexcept = 0;
        [[nodiscard]] virtual uint64_t size() const noexcept = 0;
        virtual void clear() noexcept = 0;
    };
}