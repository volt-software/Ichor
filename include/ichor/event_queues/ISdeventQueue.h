#pragma once

#ifndef ICHOR_USE_SDEVENT
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/stl/NeverAlwaysNull.h>
#include <ichor/event_queues/IEventQueue.h>

struct sd_event;

namespace Ichor {
    class ISdeventQueue : public IEventQueue {
    public:
        ~ISdeventQueue() override = default;
        [[nodiscard]] virtual NeverNull<sd_event*> getLoop() noexcept = 0;
    };
}
