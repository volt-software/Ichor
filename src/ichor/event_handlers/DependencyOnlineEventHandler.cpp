#include <ichor/DependencyManager.h>
#include <ichor/Filter.h>
#include <ichor/stl/Any.h>
#include "EventHandlerHelpers.h"

void Ichor::DependencyManager::handleEvent(DependencyOnlineEvent &evt, ProcessEventContext &) {
    auto const managerIt = _services.find(evt.originatingService);

    if (managerIt == end(_services)) [[unlikely]] {
        INTERNAL_DEBUG("DependencyOnlineEvent not found {}", evt.id);
        return;
    }

    auto &manager = managerIt->second;

    INTERNAL_DEBUG("DependencyOnlineEvent {} {} {}:{}", evt.id, evt.priority, manager->serviceId(), manager->implementationName());

    if (!manager->setInjected()) {
        INTERNAL_DEBUG("Couldn't set injected for {} {} {}", manager->serviceId(), manager->implementationName(), manager->getServiceState());
        return;
    }

    finishWaitingService(evt.originatingService, DependencyOnlineEvent::TYPE, DependencyOnlineEvent::NAME);

    if(!manager->getInterfaces().empty()) {
        auto const filterProp = manager->getProperties().find("Filter");
        const Filter *filter = nullptr;
        if (filterProp != cend(manager->getProperties())) {
            filter = Ichor::v1::any_cast<Filter const *>(&filterProp->second);
        }

        // Every service is visited, so a line per rejected service would be quadratic in the number of services. Only those that
        // request one of manager's interfaces get one, the others are counted.
        [[maybe_unused]] uint64_t filteredOut{};
        for (auto const &[serviceId, possibleDependentLifecycleManager] : _services) {
            if (serviceId == evt.originatingService) {
                continue;
            }
            if (filter != nullptr && !filter->compareTo(*possibleDependentLifecycleManager)) {
                if constexpr (DO_INTERNAL_DEBUG) {
                    if (v1::Detail::requestsAnyInterfaceOf(*possibleDependentLifecycleManager, *manager)) {
                        INTERNAL_DEBUG("DependencyOnlineEvent {} {}:{} filter '{}' rejected interested service {}:{}", evt.id, manager->serviceId(), manager->implementationName(), filter->getDescription(), serviceId, possibleDependentLifecycleManager->implementationName());
                    } else {
                        filteredOut++;
                    }
                }
                continue;
            }

            auto startBehaviour = possibleDependentLifecycleManager->dependencyOnline(manager.get());

            INTERNAL_DEBUG("DependencyOnlineEvent {} {}:{} interested service is {}:{} startBehaviour {}", evt.id, manager->serviceId(), manager->implementationName(), serviceId, possibleDependentLifecycleManager->implementationName(), startBehaviour);

            if(startBehaviour == StartBehaviour::DONE) {
                continue;
            }

            startAfterDependencyOnline(*possibleDependentLifecycleManager, evt.priority);
        }

        if constexpr (DO_INTERNAL_DEBUG) {
            if (filter != nullptr) {
                INTERNAL_DEBUG("DependencyOnlineEvent {} {}:{} filter '{}' rejected {} services not interested in it", evt.id, manager->serviceId(), manager->implementationName(), filter->getDescription(), filteredOut);
            }
        }
    }

    // If we're quitting, ensure newly started services are also stopped
    if(_quitEventReceived) {
        _eventQueue.pushPrioritisedEvent<StopServiceEvent>(manager->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt.priority), manager->serviceId(), true);
    }
}

void Ichor::DependencyManager::finishAsyncEvent(DependencyOnlineEvent &origEvt, ContinuableStartEvent const &evt, StartBehaviour result, ProcessEventContext &) {
    INTERNAL_DEBUG("Finishing handling DependencyOnlineEvent {} {} {}", origEvt.id, origEvt.priority, origEvt.originatingService);

    if(result == StartBehaviour::STARTED) {
        // The dependee went offline during the async handling of the original
        // DependencyOnlineEvent. Add a proper DependencyOfflineEvent to handle that.
        // That is, originatingService points to the dependee, not the original service that went offline.
        _eventQueue.pushPrioritisedEvent<DependencyOnlineEvent>(origEvt.originatingService, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, evt.priority));
    }
}

void Ichor::DependencyManager::startAfterDependencyOnline(ILifecycleManager &svc, uint64_t priority) {
    auto gen = svc.startAfterDependencyOnline();
    gen.set_service_id(svc.serviceId());
    gen.set_priority(std::min(svc.getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY));
    auto it = gen.begin();

    INTERNAL_DEBUG("startAfterDependencyOnline {}:{} {} {}", svc.serviceId(), svc.implementationName(), it.get_promise_id(), it.get_finished());

    if(!it.get_finished()) {
        if constexpr (DO_INTERNAL_DEBUG) {
            if (!it.get_has_suspended()) [[unlikely]] {
                std::terminate();
            }
        }
        // the DependencyOnlineEvent gets pushed by the ContinuableStartEvent handler once the coroutine finishes
        _scopedGenerators.emplace(it.get_promise_id(), ScopedGenerator{std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)), v1::make_reference_counted<DependencyOnlineEvent>(_eventQueue.getNextEventId(), svc.serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, priority))});
    } else if(it.get_value() == StartBehaviour::STARTED) {
        _eventQueue.pushPrioritisedEvent<DependencyOnlineEvent>(svc.serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, priority));
    }
}
