#include <ichor/event_queues/BaseEventQueue.h>


namespace Ichor {
    BaseEventQueue::BaseEventQueue() : _dm(*this) {
    }

    DependencyManager &BaseEventQueue::getManager() {
        return _dm;
    }

    void BaseEventQueue::startDm() {
        _dm.start();
    }

    void BaseEventQueue::processEvent(std::unique_ptr<Event> &evt) {
        _dm.processEvent(evt);
    }

    void BaseEventQueue::stopDm() {
        _dm.stop();
    }

    void BaseEventQueue::addInternalServiceManager(std::unique_ptr<ILifecycleManager> svc) {
        _dm.addInternalServiceManager(std::move(svc));
    }
}
