#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(StopServiceEvent &evt, ProcessEventContext &ctx) {
    ILifecycleManager *toStopService{};

    {
        auto const toStopServiceIt = _services.find(evt.serviceId);

        if (toStopServiceIt == end(_services)) [[unlikely]] {
            // ICHOR_LOG_ERROR(_logger, "Couldn't stop service {}, missing from known services", evt.serviceId);
            return;
        }

        toStopService = toStopServiceIt->second.get();
    }

    INTERNAL_DEBUG("StopServiceEvent {} {} {}:{} state {} dependees {} removeAfter {}", evt.id, evt.priority, evt.serviceId, toStopService->implementationName(), toStopService->getServiceState(), toStopService->getDependees().size(), evt.removeAfter);

    // already stopped
    if(toStopService->getServiceState() == ServiceState::INSTALLED) {
        INTERNAL_DEBUG("already stopped");
        return;
    }
    if(toStopService->getServiceState() == ServiceState::STOPPING) {
        INTERNAL_DEBUG("already stopping");
        return;
    }

    auto &dependencies = toStopService->getDependencies();
    auto &dependees = toStopService->getDependees();

    auto const depWaiterIt = _dependencyWaiters.find(evt.serviceId);
    if(depWaiterIt != _dependencyWaiters.end()) {
        if(!dependees.empty() || toStopService->getServiceState() != ServiceState::UNINJECTING) {
            INTERNAL_DEBUG("existing dependency offline waiter {} {} {}", dependees.size(), toStopService->getServiceState(), depWaiterIt->second.events.size());
            if constexpr (DO_INTERNAL_DEBUG) {
                for ([[maybe_unused]] auto serviceId : dependees) {
                    INTERNAL_DEBUG("dependee: {}", serviceId);
                }
            }
            return;
        }

        INTERNAL_DEBUG("waitForService done {} {} {}", evt.serviceId, DependencyOfflineEvent::NAME, depWaiterIt->second.events.size());

        finishWaitingService(evt.serviceId, DependencyOfflineEvent::TYPE, DependencyOfflineEvent::NAME);
    }

    // coroutine needs to finish before we can stop the service
    if(existingCoroutineFor(evt.serviceId)) {
        _pendingStopsDueToCoroutine.emplace(evt.serviceId, WaitingStopService{evt.originatingService, evt.priority, evt.removeAfter, {}});
        INTERNAL_DEBUG("existing scoped event");
        return;
    }

    if constexpr (DO_INTERNAL_DEBUG) {
        for ([[maybe_unused]] auto serviceId : dependencies) {
            INTERNAL_DEBUG("dependency: {}", serviceId);
        }

        for ([[maybe_unused]] auto serviceId : dependees) {
            INTERNAL_DEBUG("dependee: {}", serviceId);
        }
    }

    // If all services depending on this one have removed this one and we've had a DependencyOfflineEvent
    if (dependees.empty() && toStopService->getServiceState() == ServiceState::UNINJECTING) {
        if(_logger != nullptr && evt.serviceId == _logger->getFrameworkServiceId()) {
            _logger = nullptr;
        }

        auto gen = toStopService->stop();
        gen.set_service_id(toStopService->serviceId());
        gen.set_priority(std::min(toStopService->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY));
        auto it = gen.begin();

        if (!it.get_finished()) {
            if constexpr (DO_INTERNAL_DEBUG || DO_INTERNAL_COROUTINE_DEBUG) {
                if (!it.get_has_suspended()) [[unlikely]] {
                    std::terminate();
                }
            }
            auto prom_id = it.get_promise_id();
            INTERNAL_DEBUG("StopServiceEvent contains {} {} {}", prom_id, _scopedGenerators.contains(prom_id),
                           _scopedGenerators.size() + 1);
            auto scopedIt =_scopedGenerators.emplace(prom_id, ScopedGenerator{std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)), std::move(ctx.uniqueEvt)});
            // auto scopedIt = _scopedEvents.emplace(prom_id, std::move(ctx.uniqueEvt));
            ctx.evt = scopedIt.first->second.event.get();
            return;
        }

        if constexpr (DO_INTERNAL_DEBUG || DO_INTERNAL_COROUTINE_DEBUG) {
            if (it.get_has_suspended()) [[unlikely]] {
                INTERNAL_DEBUG("{}", it.get_promise_id());
                std::terminate();
            }
        }

        INTERNAL_DEBUG("service->stop() {}:{} state {}", evt.serviceId, toStopService->implementationName(), toStopService->getServiceState());
        if (toStopService->getServiceState() != ServiceState::INSTALLED) [[unlikely]] {
            std::terminate();
        }

        finishStoppingService(evt, *toStopService, ctx);
        checkIfCanQuit(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy);
    } else if(toStopService->getServiceState() != ServiceState::UNINJECTING) {
        // Trigger dependency offline handling once; StopServiceEvent will be (re)scheduled
        // after all DependencyOfflineEvents have completed to avoid event storms.
        // Multiple StopServiceEvents may be queued for the same service, only push one DependencyOfflineEvent for them.
        if(_pendingDependencyOfflines.emplace(evt.serviceId).second) {
            _eventQueue.pushPrioritisedEvent<DependencyOfflineEvent>(evt.serviceId, evt.priority, evt.removeAfter);
        }
    }
}

void Ichor::DependencyManager::finishAsyncEvent(StopServiceEvent &origEvt, ContinuableStartEvent const &, StartBehaviour, ProcessEventContext &ctx) {
    INTERNAL_DEBUG("Finishing handling StopServiceEvent {} {} {} {} {}", origEvt.id, origEvt.priority, origEvt.originatingService, origEvt.serviceId, origEvt.removeAfter);

    //finishWaitingService(origEvt.serviceId, StopServiceEvent::TYPE, StopServiceEvent::NAME);
    auto const serviceIt = _services.find(origEvt.serviceId);

    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if(serviceIt == _services.end()) [[unlikely]] {
            ICHOR_EMERGENCY_LOG2(_logger, "When trying to finish stopping an async service, couldn't find the service by its id. {}", origEvt.serviceId);
            std::terminate();
        }
        if(serviceIt->second->getServiceState() != ServiceState::INSTALLED) [[unlikely]] {
            ICHOR_EMERGENCY_LOG2(_logger, "When trying to finish stopping async service {}:{}, service wasn't in the correct state {}", origEvt.serviceId, serviceIt->second->implementationName(), serviceIt->second->getServiceState());
            std::terminate();
        }
        if(!serviceIt->second->getDependees().empty()) [[unlikely]] {
            ICHOR_EMERGENCY_LOG2(_logger, "When trying to finish stopping async service {}:{}, the service still had dependendees: ", origEvt.serviceId, serviceIt->second->implementationName());
            for([[maybe_unused]] auto dep : serviceIt->second->getDependees()) {
                INTERNAL_DEBUG("dep: {}", dep);
            }
            std::terminate();
        }
    }

    finishStoppingService(origEvt, *serviceIt->second, ctx);
}

void Ichor::DependencyManager::finishStoppingService(StopServiceEvent const &evt, ILifecycleManager &svc, ProcessEventContext &ctx) {
    auto pendingSvcIt = _pendingStopsDueToDependencies.find(evt.serviceId);

    if(pendingSvcIt != _pendingStopsDueToDependencies.end()) {
        for(auto const &pendingSvcInfo : pendingSvcIt->second) {
            auto pendingSvc = _services.find(pendingSvcInfo.originatingServiceId);

            if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
                if(pendingSvc == _services.end()) [[unlikely]] {
                    INTERNAL_DEBUG("StopServiceEvent {} {} {}:-- state -- dependee {}:{} state {} pending service {} missing. Catastrophic error.", evt.id, evt.priority, pendingSvcInfo.originatingServiceId, svc.serviceId(), svc.implementationName(), svc.getServiceState(), pendingSvcInfo.originatingServiceId);
                    std::terminate();
                }
            }

            svc.finishDependencyOffline(pendingSvc->second.get(), pendingSvcInfo.deps);

            if(pendingSvc->second->getDependees().empty()) {
                INTERNAL_DEBUG("StopServiceEvent {} {} {}:{} state {} dependee {}:{} state {} queueing stop for {}:{}", evt.id, evt.priority, pendingSvc->second->serviceId(), pendingSvc->second->implementationName(), pendingSvc->second->getServiceState(), svc.serviceId(), svc.implementationName(), svc.getServiceState(), pendingSvc->second->serviceId(), pendingSvc->second->implementationName());
                _eventQueue.pushPrioritisedEvent<StopServiceEvent>(evt.serviceId, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, evt.priority), pendingSvcInfo.originatingServiceId, pendingSvcInfo.removeAfter);
            }
        }

        _pendingStopsDueToDependencies.erase(pendingSvcIt);
    }

    auto &dependencies = svc.getDependencies();
    for (auto serviceId : dependencies) {
        auto const depIt = _services.find(serviceId);

        if (depIt == _services.end()) {
            continue;
        }

        depIt->second->getDependees().erase(evt.serviceId);
    }
    dependencies.clear();
    clearServiceRegistrations(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy, evt.serviceId);
    // service is stopped, a stop that was postponed due to existing coroutines is no longer relevant
    _pendingStopsDueToCoroutine.erase(evt.serviceId);

    INTERNAL_DEBUG("service->stop() {}:{} removeAfter {}", evt.serviceId, svc.implementationName(), evt.removeAfter);
    // removes svc, it must not be used after this
    if(evt.removeAfter) {
        removeInternalService(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy, evt.serviceId);
    }
}
