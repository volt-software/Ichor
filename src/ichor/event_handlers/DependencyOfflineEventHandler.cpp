#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(DependencyOfflineEvent &evt, ProcessEventContext &ctx) {
    _pendingDependencyOfflines.erase(evt.originatingService);
    auto const managerIt = _services.find(evt.originatingService);

    if (managerIt == end(_services)) [[unlikely]] {
        return;
    }

    auto &manager = managerIt->second;
    INTERNAL_DEBUG("DependencyOfflineEvent {} {} {}:{} {} {}", evt.id, evt.priority, evt.originatingService, manager->implementationName(), manager->getServiceState(), evt.removeOriginatingServiceAfterStop);

    if (!manager->setUninjected()) {
        // DependencyOfflineEvent already started or processed
        INTERNAL_DEBUG("Couldn't set uninjected for {} {} {}", manager->serviceId(), manager->implementationName(), manager->getServiceState());
        return;
    }

    // copy dependees as it will be modified during this loop
    auto dependees = manager->getDependees();
    for (auto &serviceId : dependees) {
        auto const depIt = _services.find(serviceId);

        if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
            if (depIt == _services.end()) [[unlikely]] {
                ICHOR_EMERGENCY_LOG2(_logger, "Couldn't find dependendee {}.", serviceId);
                std::terminate();
            }
        }

        auto priority = std::min(depIt->second->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY);
        auto depIts = depIt->second->interestedInDependencyGoingOffline(manager.get());

        if(depIts.empty()) {
            continue;
        }

        auto gen = depIt->second->dependencyOffline(manager.get(), depIts);
        gen.set_service_id(depIt->second->serviceId());
        gen.set_priority(priority);
        auto it = gen.begin();

        if(!it.get_finished()) {
            if constexpr (DO_INTERNAL_DEBUG) {
                if (!it.get_has_suspended()) [[unlikely]] {
                    INTERNAL_DEBUG("{}", it.get_promise_id());
                    std::terminate();
                }
            }
            _scopedGenerators.emplace(it.get_promise_id(), ScopedGenerator{std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)), v1::make_reference_counted<ContinuableDependencyOfflineEvent>(_eventQueue.getNextEventId(), serviceId, priority, evt.originatingService, evt.removeOriginatingServiceAfterStop, std::move(depIts))});
            // create new event that will be inserted upon finish of coroutine in ContinuableStartEvent
            // _scopedEvents.emplace(it.get_promise_id(), v1::make_reference_counted<ContinuableDependencyOfflineEvent>(_eventQueue.getNextEventId(), serviceId, priority, evt.originatingService, evt.removeOriginatingServiceAfterStop, std::move(depIts)));
            continue;
        }

        if constexpr (DO_INTERNAL_DEBUG) {
            if (it.get_has_suspended()) [[unlikely]] {
                INTERNAL_DEBUG("{}", it.get_promise_id());
                std::terminate();
            }
        }

        // Dependee needs to be stopped, we need to keep originatingService alive until all dependees that need to be stopped are stopped.
        if(it.get_value() == StartBehaviour::STOP_ME) {
            INTERNAL_DEBUG("DependencyOfflineEvent {} {} {}:{} state {} dependee {}:{} state {} dependee requesting stop", evt.id, evt.priority, evt.originatingService, manager->implementationName(), manager->getServiceState(), serviceId, depIt->second->implementationName(), depIt->second->getServiceState());
            auto& waitingSvcs = _pendingStopsDueToDependencies[serviceId];
            if(waitingSvcs.empty()) {
                _eventQueue.pushPrioritisedEvent<StopServiceEvent>(evt.originatingService, priority, serviceId, evt.removeOriginatingServiceAfterStop);
            }
            waitingSvcs.emplace_back(evt.originatingService, priority, evt.removeOriginatingServiceAfterStop, std::move(depIts));
            continue;
        }

        if(it.get_value() == StartBehaviour::STOPPED) [[unlikely]] {
            INTERNAL_DEBUG("DependencyOfflineEvent {} {} {}:{} state {} dependee {}:{} state {} dependee stopped?", evt.id, evt.priority, evt.originatingService, manager->implementationName(), manager->getServiceState(), serviceId, depIt->second->implementationName(), depIt->second->getServiceState());
            // depIt->second->finishDependencyOffline(manager.get(), depIts);
            // clearServiceRegistrations(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy, serviceId);
            // //finishWaitingService(serviceId, StopServiceEvent::TYPE, StopServiceEvent::NAME);
            // _eventQueue.pushPrioritisedEvent<DependencyOfflineEvent>(serviceId, priority, false);
            std::terminate();
        }

        if(it.get_value() == StartBehaviour::DONE) {
            INTERNAL_DEBUG("DependencyOfflineEvent {} {} {}:{} state {} dependee {}:{} state {} dependee removed", evt.id, evt.priority, evt.originatingService, manager->implementationName(), manager->getServiceState(), serviceId, depIt->second->implementationName(), depIt->second->getServiceState());
            depIt->second->finishDependencyOffline(manager.get(), depIts);
        }

        if constexpr (DO_INTERNAL_DEBUG) {
            if(it.get_value() == StartBehaviour::STARTED) [[unlikely]] {
                INTERNAL_DEBUG("DependencyOfflineEvent {} {} {}:{} state {} dependee {}:{} state {} dependee catastrophic failure", evt.id, evt.priority, evt.originatingService, manager->implementationName(), manager->getServiceState(), serviceId, depIt->second->implementationName(), depIt->second->getServiceState());
                std::terminate();
            }
        }
    }
    if(manager->getDependees().empty()) {
        finishWaitingService(evt.originatingService, DependencyOfflineEvent::TYPE, DependencyOfflineEvent::NAME);
        _eventQueue.pushPrioritisedEvent<StopServiceEvent>(evt.originatingService, std::min(evt.priority, INTERNAL_DEPENDENCY_EVENT_PRIORITY), evt.originatingService, evt.removeOriginatingServiceAfterStop);
    }
}

void Ichor::DependencyManager::finishAsyncEvent(ContinuableDependencyOfflineEvent &origEvt, ContinuableStartEvent const &evt, StartBehaviour result, ProcessEventContext &) {
    INTERNAL_DEBUG("Finishing handling ContinuableDependencyOfflineEvent {} {} {} {} {}", origEvt.id, origEvt.priority, origEvt.originatingService, origEvt.originatingOfflineServiceId, origEvt.removeOriginatingOfflineServiceAfterStop);

    // the originatingService is actually from a dependee of the originatingOfflineServiceId, so originatingOfflineServiceId is the service that is interested in this dependency going offline
    // We need to check if all its dependees are handled and if so, finish its offline waiter
    auto const originatingOfflineServiceIt = _services.find(origEvt.originatingOfflineServiceId);
    auto const serviceIt = _services.find(origEvt.originatingService);

    if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
        if(originatingOfflineServiceIt == _services.end()) [[unlikely]] {
            std::terminate();
        }

        if(serviceIt == _services.end()) [[unlikely]] {
            std::terminate();
        }
    }

#ifdef ICHOR_ENABLE_INTERNAL_DEBUGGING
    auto &deps = originatingOfflineServiceIt->second->getDependees();
    INTERNAL_DEBUG("originatingOfflineService {}:{} {} {}", origEvt.originatingOfflineServiceId, originatingOfflineServiceIt->second->implementationName(), originatingOfflineServiceIt->second->getServiceState(), deps.size());
    for(auto dep : deps) {
        INTERNAL_DEBUG("dep: {}", dep);
    }
#endif

    if(result == StartBehaviour::STOP_ME) {
        INTERNAL_DEBUG("ContinuableDependencyOfflineEvent {} {} {}:{} state {} dependee {}:{} state {} dependee requesting stop.", evt.id, evt.priority, originatingOfflineServiceIt->second->serviceId(), originatingOfflineServiceIt->second->implementationName(), originatingOfflineServiceIt->second->getServiceState(), serviceIt->second->serviceId(), serviceIt->second->implementationName(), serviceIt->second->getServiceState());
        auto& waitingSvcs = _pendingStopsDueToDependencies[origEvt.originatingService];
        if(waitingSvcs.empty() || (serviceIt->second->getServiceState() == ServiceState::UNINJECTING && serviceIt->second->getDependees().empty())) {
            _eventQueue.pushPrioritisedEvent<StopServiceEvent>(evt.originatingService, evt.priority, origEvt.originatingService, origEvt.removeOriginatingOfflineServiceAfterStop);
        }
        waitingSvcs.emplace_back(origEvt.originatingOfflineServiceId, evt.priority, origEvt.removeOriginatingOfflineServiceAfterStop, std::move(origEvt.dependencyIterators));
        return;
    }

    if constexpr (DO_INTERNAL_DEBUG) {
        if(result == StartBehaviour::STOPPED) [[unlikely]] {
            INTERNAL_DEBUG("ContinuableDependencyOfflineEvent {} {} {}:{} state {} dependee {}:{} state {} dependee stopped?", evt.id, evt.priority, originatingOfflineServiceIt->second->serviceId(), originatingOfflineServiceIt->second->implementationName(), originatingOfflineServiceIt->second->getServiceState(), serviceIt->second->serviceId(), serviceIt->second->implementationName(), serviceIt->second->getServiceState());
            // serviceIt->second->finishDependencyOffline(originatingOfflineServiceIt->second.get(), origEvt.dependencyIterators);
            // clearServiceRegistrations(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy, origEvt.originatingService);
            // //finishWaitingService(origEvt.originatingService, StopServiceEvent::TYPE, StopServiceEvent::NAME);
            // // The dependee of originatingOfflineServiceId went offline during the async handling of the original
            // // DependencyOfflineEvent. Add a proper DependencyOfflineEvent to handle that.
            // _eventQueue.pushPrioritisedEvent<DependencyOfflineEvent>(origEvt.originatingService, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, evt.priority), origEvt.removeOriginatingOfflineServiceAfterStop);
            std::terminate();
        }
        if(result == StartBehaviour::STARTED) [[unlikely]] {
            INTERNAL_DEBUG("ContinuableDependencyOfflineEvent {} {} {}:{} state {} dependee {}:{} state {} dependee somehow started. Catastrophic failure.", evt.id, evt.priority, originatingOfflineServiceIt->second->serviceId(), originatingOfflineServiceIt->second->implementationName(), originatingOfflineServiceIt->second->getServiceState(), serviceIt->second->serviceId(), serviceIt->second->implementationName(), serviceIt->second->getServiceState());
            std::terminate();
        }
    }

    INTERNAL_DEBUG("ContinuableDependencyOfflineEvent {} {} {}:{} state {} dependee {}:{} state {} dependee DONE", evt.id, evt.priority, originatingOfflineServiceIt->second->serviceId(), originatingOfflineServiceIt->second->implementationName(), originatingOfflineServiceIt->second->getServiceState(), serviceIt->second->serviceId(), serviceIt->second->implementationName(), serviceIt->second->getServiceState());
    serviceIt->second->finishDependencyOffline(originatingOfflineServiceIt->second.get(), origEvt.dependencyIterators);

    if(originatingOfflineServiceIt->second->getDependees().empty()) {
        INTERNAL_DEBUG("originatingOfflineService found waiting service {} {}", origEvt.originatingOfflineServiceId, originatingOfflineServiceIt->second->getServiceState());
        // Service needs to be stopped to complete the sequence
        _eventQueue.pushPrioritisedEvent<StopServiceEvent>(origEvt.originatingOfflineServiceId, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, evt.priority), origEvt.originatingOfflineServiceId, origEvt.removeOriginatingOfflineServiceAfterStop);
        finishWaitingService(origEvt.originatingOfflineServiceId, DependencyOfflineEvent::TYPE, DependencyOfflineEvent::NAME);
    }
}
