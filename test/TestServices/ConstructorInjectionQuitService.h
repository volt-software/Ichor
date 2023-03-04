#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/event_queues/IEventQueue.h>

using namespace Ichor;

struct ConstructorInjectionQuitService final {
    ConstructorInjectionQuitService(IEventQueue *q) {
        if(q == nullptr) {
            std::terminate();
        }

        q->pushEvent<QuitEvent>(0);
    }
};
