#include <ichor/dependency_management/DependencyRegistrations.h>
#include <ichor/event_queues/IEventQueue.h>

namespace Ichor::Detail {
    constinit thread_local extern DependencyManager *_local_dm;
}

Ichor::EventHandlerRegistration::~EventHandlerRegistration() {
    reset();
}

void Ichor::EventHandlerRegistration::reset() {
    if(_key.type != 0) {
        Ichor::GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveEventHandlerEvent>(_key.id, _priority, _key);
        _key.type = 0;
    }
}

Ichor::EventInterceptorRegistration::~EventInterceptorRegistration() {
    reset();
}

void Ichor::EventInterceptorRegistration::reset() {
    if(Detail::_local_dm != nullptr && _interceptorId != 0) {
        Ichor::GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveEventInterceptorEvent>(_listeningServiceId, _priority, _interceptorId, _eventType);
        _interceptorId = 0;
    }
}

Ichor::DependencyTrackerRegistration::~DependencyTrackerRegistration() {
    reset();
}

void Ichor::DependencyTrackerRegistration::reset() {
    if(_interfaceNameHash != 0) {
        Ichor::GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveTrackerEvent>(_serviceId, _priority, _interfaceNameHash);
        _interfaceNameHash = 0;
    }
}
