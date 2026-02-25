#pragma once

#include <ichor/event_queues/IEventQueue.h>
#include <ichor/DependencyManager.h>

namespace Ichor {
    class BaseEventQueue : public IEventQueue {
    public:
        BaseEventQueue();
        ~BaseEventQueue() override = default;

        DependencyManager& getManager() override;

    protected:
        void startDm();
        void processEvent(std::unique_ptr<Event> &evt);
        void stopDm();
        void addInternalServiceManager(std::unique_ptr<ILifecycleManager> svc);

        DependencyManager _dm;
    };
}
