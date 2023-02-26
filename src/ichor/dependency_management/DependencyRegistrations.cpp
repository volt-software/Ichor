#include <ichor/dependency_management/DependencyRegistrations.h>
#include <ichor/event_queues/IEventQueue.h>

Ichor::EventCompletionHandlerRegistration::~EventCompletionHandlerRegistration() {
    if(_key.type != 0) {
        Ichor::GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveCompletionCallbacksEvent>(_key.id, _priority, _key);
        _key.type = 0;
    }
}

void Ichor::EventCompletionHandlerRegistration::reset() {
    if(_key.type != 0) {
        Ichor::GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveCompletionCallbacksEvent>(_key.id, _priority, _key);
        _key.type = 0;
    }
}

Ichor::EventHandlerRegistration::~EventHandlerRegistration() {
    if(_key.type != 0) {
        Ichor::GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveEventHandlerEvent>(_key.id, _priority, _key);
        _key.type = 0;
    }
}

void Ichor::EventHandlerRegistration::reset() {
    if(_key.type != 0) {
        Ichor::GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveEventHandlerEvent>(_key.id, _priority, _key);
        _key.type = 0;
    }
}

Ichor::EventInterceptorRegistration::~EventInterceptorRegistration() {
    if(_key.type != 0) {
        Ichor::GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveEventInterceptorEvent>(_key.id, _priority, _key);
    }
}

void Ichor::EventInterceptorRegistration::reset() {
    if(_key.type != 0) {
        Ichor::GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveEventInterceptorEvent>(_key.id, _priority, _key);
        _key.type = 0;
    }
}

Ichor::DependencyTrackerRegistration::~DependencyTrackerRegistration() {
    if(_interfaceNameHash != 0) {
        Ichor::GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveTrackerEvent>(_serviceId, _priority, _interfaceNameHash);
        _interfaceNameHash = 0;
    }
}

void Ichor::DependencyTrackerRegistration::reset() {
    if(_interfaceNameHash != 0) {
        Ichor::GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveTrackerEvent>(_serviceId, _priority, _interfaceNameHash);
        _interfaceNameHash = 0;
    }
}
