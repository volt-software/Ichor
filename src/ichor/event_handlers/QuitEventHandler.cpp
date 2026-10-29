#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(QuitEvent &evt, ProcessEventContext &ctx) {
    INTERNAL_DEBUG("QuitEvent {} {} {}", evt.id, evt.priority, evt.originatingService);

    _quitEventReceived = true;

    uint64_t lowest_priority{evt.priority};

    for (auto const &[key, possibleManager]: _services) {
        if (possibleManager->getServiceState() == ServiceState::ACTIVE) {
            if(possibleManager->isInternalManager()) {
                continue;
            }

            auto priority = std::max(possibleManager->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY);

            if(priority > lowest_priority) {
                lowest_priority = priority;
            }

            // Dependees are not stopped here explicitly: they get stopped through the DependencyOfflineEvent (STOP_ME) or their own StopServiceEvent.
            _eventQueue.pushPrioritisedEvent<StopServiceEvent>(evt.originatingService, priority, possibleManager->serviceId(), true);
        }

        if(possibleManager->getServiceState() != ServiceState::INSTALLED) {
            if constexpr (DO_INTERNAL_DEBUG) {
                INTERNAL_DEBUG("Service {}:{} {}", key, possibleManager->implementationName(), possibleManager->getServiceState());
                for ([[maybe_unused]] auto dep: possibleManager->getDependees()) {
                    INTERNAL_DEBUG("dependee: {}", dep);
                }
                if (existingCoroutineFor(key)) {
                    INTERNAL_DEBUG("Existing coroutine");
                }
                auto const waiter = _dependencyWaiters.find(key);
                if (waiter != _dependencyWaiters.end()) {
                    INTERNAL_DEBUG("Existing dependency offline waiter {} {} {}", waiter->second.waitingSvcId, waiter->second.eventType, waiter->second.events.size());
                }
            }
        }
    }

    checkIfCanQuit(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy);
}
