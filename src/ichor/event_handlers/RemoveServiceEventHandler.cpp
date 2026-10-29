#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(RemoveServiceEvent &evt, ProcessEventContext &ctx) {
    auto const toRemoveServiceIt = _services.find(evt.serviceId);

    if (toRemoveServiceIt == end(_services)) [[unlikely]] {
        ICHOR_LOG_ERROR(_logger, "Couldn't remove service {}, missing from known services", evt.serviceId);
        return;
    }

    auto *toRemoveService = toRemoveServiceIt->second.get();
    INTERNAL_DEBUG("RemoveServiceEvent {} {} {}:{} {}", evt.id, evt.priority, toRemoveService->serviceId(), toRemoveService->implementationName(), toRemoveService->getServiceState());

    if(toRemoveService->getServiceState() != ServiceState::INSTALLED || !toRemoveService->getDependees().empty() || !toRemoveService->getDependencies().empty()) {
        INTERNAL_DEBUG("Couldn't remove service {}, not stopped first {} {}", evt.serviceId, toRemoveService->getDependees().size(), toRemoveService->getDependencies().size());
        ICHOR_LOG_ERROR(_logger, "Couldn't remove service {}, not stopped first", evt.serviceId);
        return;
    }

    removeInternalService(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy, toRemoveServiceIt->first);
}
