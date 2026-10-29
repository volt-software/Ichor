#include <ichor/DependencyManager.h>
#include <ichor/events/RunFunctionEvent.h>

void Ichor::DependencyManager::handleEvent(RunFunctionEvent &evt, ProcessEventContext &) {
    INTERNAL_DEBUG("RunFunctionEvent {} {}", evt.id, evt.priority);

    // Do not handle stale run function events
    if(evt.originatingService != ServiceIdType{0}) {
        auto const requestingServiceIt = _services.find(evt.originatingService);
        if(requestingServiceIt == end(_services)) {
            INTERNAL_DEBUG("Service {} not found", evt.originatingService);
            return;
        }
        if(requestingServiceIt->second->getServiceState() == ServiceState::INSTALLED) {
            INTERNAL_DEBUG("Service {}:{} not active", evt.originatingService, requestingServiceIt->second->implementationName());
            return;
        }
    }

    evt.fun();
}
