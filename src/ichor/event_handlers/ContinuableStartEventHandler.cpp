#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(ContinuableStartEvent &evt, ProcessEventContext &ctx) {
    INTERNAL_DEBUG("ContinuableStartEvent {} {} {}", evt.promiseId, evt.id, evt.priority);
    auto const genIt = _scopedGenerators.find(evt.promiseId);

    if (genIt != _scopedGenerators.end()) {
        INTERNAL_DEBUG("ContinuableStartEvent {}", genIt->second.generator->done());

        StartBehaviour it_ret{};
        if (!genIt->second.generator->done()) {
            auto it = genIt->second.generator->begin_interface();
            INTERNAL_DEBUG("ContinuableStartEvent it {} {} {}", it->get_finished(), it->get_op_state(), it->get_promise_state());

            if (!it->get_finished()) {
                if constexpr (DO_INTERNAL_DEBUG || DO_INTERNAL_COROUTINE_DEBUG) {
                    if (!it->get_has_suspended()) [[unlikely]] {
                        INTERNAL_DEBUG("{}", it->get_promise_id());
                        std::terminate();
                    }
                }
                return;
            }

            it_ret = static_cast<Ichor::v1::Detail::AsyncGeneratorBeginOperation<StartBehaviour>*>(it.get())->get_value();
        } else {
            it_ret = static_cast<AsyncGenerator<StartBehaviour>*>(genIt->second.generator.get())->get_value();
        }

        INTERNAL_DEBUG("ContinuableStartEvent removed {} {} {}", it_ret, evt.promiseId, _scopedGenerators.size() - 1);

        auto origEvtType = genIt->second.event->get_type();
        bool erasedScopedEntries{false};

        if(origEvtType == StartServiceEvent::TYPE) {
            auto const origEvt = static_cast<StartServiceEvent*>(genIt->second.event.get());

            INTERNAL_DEBUG("Finishing handling StartServiceEvent {} {} {} {}", origEvt->id, origEvt->priority, origEvt->originatingService, origEvt->serviceId);

            _eventQueue.pushPrioritisedEvent<DependencyOnlineEvent>(origEvt->serviceId, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, evt.priority));
            // If we're quitting, ensure newly started services are also stopped
            if(_quitEventReceived) {
                _eventQueue.pushPrioritisedEvent<StopServiceEvent>(origEvt->serviceId, std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt.priority), origEvt->serviceId, true);
            }
            handleEventCompletion(*origEvt);
        } else if(origEvtType == StopServiceEvent::TYPE) {
            auto origEvt = static_cast<StopServiceEvent *>(genIt->second.event.get());

            INTERNAL_DEBUG("Finishing handling StopServiceEvent {} {} {} {} {}", origEvt->id, origEvt->priority, origEvt->originatingService, origEvt->serviceId, origEvt->removeAfter);

            //finishWaitingService(origEvt->serviceId, StopServiceEvent::TYPE, StopServiceEvent::NAME);
            auto const serviceIt = _services.find(origEvt->serviceId);

            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if(serviceIt == _services.end()) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG2(_logger, "When trying to finish stopping an async service, couldn't find the service by its id. {}", origEvt->serviceId);
                    std::terminate();
                }
                if(serviceIt->second->getServiceState() != ServiceState::INSTALLED) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG2(_logger, "When trying to finish stopping async service {}:{}, service wasn't in the correct state {}", origEvt->serviceId, serviceIt->second->implementationName(), serviceIt->second->getServiceState());
                    std::terminate();
                }
                if(!serviceIt->second->getDependees().empty()) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG2(_logger, "When trying to finish stopping async service {}:{}, the service still had dependendees: ", origEvt->serviceId, serviceIt->second->implementationName());
                    for([[maybe_unused]] auto dep : serviceIt->second->getDependees()) {
                        INTERNAL_DEBUG("dep: {}", dep);
                    }
                    std::terminate();
                }
            }

            auto pendingSvcIt = _pendingStopsDueToDependencies.find(origEvt->serviceId);

            if(pendingSvcIt != _pendingStopsDueToDependencies.end()) {
                for(auto const &pendingSvcInfo : pendingSvcIt->second) {
                    auto pendingSvc = _services.find(pendingSvcInfo.originatingServiceId);

                    if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
                        if(pendingSvc == _services.end()) [[unlikely]] {            
                            INTERNAL_DEBUG("Continuable StopServiceEvent {} {} {}:-- state -- dependee {}:{} state {} pending service {} missing. Catastrophic error.", origEvt->id, origEvt->priority, pendingSvcInfo.originatingServiceId, serviceIt->second->serviceId(), serviceIt->second->implementationName(), serviceIt->second->getServiceState(), pendingSvcInfo.originatingServiceId);
                            std::terminate();
                        }
                    }

                    serviceIt->second->finishDependencyOffline(pendingSvc->second.get(), pendingSvcInfo.deps);

                    if(pendingSvc->second->getDependees().empty()) {
                        INTERNAL_DEBUG("Continuable StopServiceEvent {} {} {}:{} state {} dependee {}:{} state {} queueing stop for {}:{}", origEvt->id, origEvt->priority, pendingSvc->second->serviceId(), pendingSvc->second->implementationName(), pendingSvc->second->getServiceState(), serviceIt->second->serviceId(), serviceIt->second->implementationName(), serviceIt->second->getServiceState(), pendingSvc->second->serviceId(), pendingSvc->second->implementationName());
                        _eventQueue.pushPrioritisedEvent<StopServiceEvent>(origEvt->serviceId, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, origEvt->priority), pendingSvcInfo.originatingServiceId, pendingSvcInfo.removeAfter);
                    }
                }

                _pendingStopsDueToDependencies.erase(pendingSvcIt);
            }

            auto &dependencies = serviceIt->second->getDependencies();
            for (auto serviceId : dependencies) {
                auto const depIt = _services.find(serviceId);

                if (depIt == _services.end()) {
                    continue;
                }

                depIt->second->getDependees().erase(origEvt->serviceId);
            }
            dependencies.clear();

            clearServiceRegistrations(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy, origEvt->serviceId);
            // service is stopped, a stop that was postponed due to existing coroutines is no longer relevant
            _pendingStopsDueToCoroutine.erase(origEvt->serviceId);
            if(origEvt->removeAfter) {
                removeInternalService(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy, origEvt->serviceId);
            }

            handleEventCompletion(*origEvt);

            _scopedGenerators.erase(genIt);
            erasedScopedEntries = true;

            checkIfCanQuit(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy);
        } else if(origEvtType == DependencyOnlineEvent::TYPE) {
            auto origEvt = static_cast<DependencyOnlineEvent *>(genIt->second.event.get());

            INTERNAL_DEBUG("Finishing handling DependencyOnlineEvent {} {} {}", origEvt->id, origEvt->priority, origEvt->originatingService);

            if(it_ret == StartBehaviour::STARTED) {
                // The dependee went offline during the async handling of the original
                // DependencyOnlineEvent. Add a proper DependencyOfflineEvent to handle that.
                // That is, originatingService points to the dependee, not the original service that went offline.
                _eventQueue.pushPrioritisedEvent<DependencyOnlineEvent>(origEvt->originatingService, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, evt.priority));
            }
            handleEventCompletion(*origEvt);
        } else if(origEvtType == ContinuableDependencyOfflineEvent::TYPE) {
            auto origEvt = static_cast<ContinuableDependencyOfflineEvent *>(genIt->second.event.get());

            INTERNAL_DEBUG("Finishing handling ContinuableDependencyOfflineEvent {} {} {} {} {}", origEvt->id, origEvt->priority, origEvt->originatingService, origEvt->originatingOfflineServiceId, origEvt->removeOriginatingOfflineServiceAfterStop);


            // the originatingService is actually from a dependee of the originatingOfflineServiceId, so originatingOfflineServiceId is the service that is interested in this dependency going offline
            // We need to check if all its dependees are handled and if so, finish its offline waiter
            auto const originatingOfflineServiceIt = _services.find(origEvt->originatingOfflineServiceId);
            auto const serviceIt = _services.find(origEvt->originatingService);

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
            INTERNAL_DEBUG("originatingOfflineService {}:{} {} {}", origEvt->originatingOfflineServiceId, originatingOfflineServiceIt->second->implementationName(), originatingOfflineServiceIt->second->getServiceState(), deps.size());
            for(auto dep : deps) {
                INTERNAL_DEBUG("dep: {}", dep);
            }
#endif

            if(it_ret == StartBehaviour::STOP_ME) {
                INTERNAL_DEBUG("ContinuableDependencyOfflineEvent {} {} {}:{} state {} dependee {}:{} state {} dependee requesting stop.", evt.id, evt.priority, originatingOfflineServiceIt->second->serviceId(), originatingOfflineServiceIt->second->implementationName(), originatingOfflineServiceIt->second->getServiceState(), serviceIt->second->serviceId(), serviceIt->second->implementationName(), serviceIt->second->getServiceState());
                auto& waitingSvcs = _pendingStopsDueToDependencies[origEvt->originatingService];
                if(waitingSvcs.empty() || (serviceIt->second->getServiceState() == ServiceState::UNINJECTING && serviceIt->second->getDependees().empty())) {
                    _eventQueue.pushPrioritisedEvent<StopServiceEvent>(evt.originatingService, evt.priority, origEvt->originatingService, origEvt->removeOriginatingOfflineServiceAfterStop);
                }
                waitingSvcs.emplace_back(origEvt->originatingOfflineServiceId, evt.priority, origEvt->removeOriginatingOfflineServiceAfterStop, std::move(origEvt->dependencyIterators));
                
            } else {
                if constexpr (DO_INTERNAL_DEBUG) {
                    if(it_ret == StartBehaviour::STOPPED) [[unlikely]] {
                        INTERNAL_DEBUG("ContinuableDependencyOfflineEvent {} {} {}:{} state {} dependee {}:{} state {} dependee stopped?", evt.id, evt.priority, originatingOfflineServiceIt->second->serviceId(), originatingOfflineServiceIt->second->implementationName(), originatingOfflineServiceIt->second->getServiceState(), serviceIt->second->serviceId(), serviceIt->second->implementationName(), serviceIt->second->getServiceState());
                        // serviceIt->second->finishDependencyOffline(originatingOfflineServiceIt->second.get(), origEvt->dependencyIterators);
                        // clearServiceRegistrations(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy, origEvt->originatingService);
                        // //finishWaitingService(origEvt->originatingService, StopServiceEvent::TYPE, StopServiceEvent::NAME);
                        // // The dependee of originatingOfflineServiceId went offline during the async handling of the original
                        // // DependencyOfflineEvent. Add a proper DependencyOfflineEvent to handle that.
                        // _eventQueue.pushPrioritisedEvent<DependencyOfflineEvent>(origEvt->originatingService, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, evt.priority), origEvt->removeOriginatingOfflineServiceAfterStop);
                        std::terminate();
                    }
                    if(it_ret == StartBehaviour::STARTED) [[unlikely]] {
                        INTERNAL_DEBUG("ContinuableDependencyOfflineEvent {} {} {}:{} state {} dependee {}:{} state {} dependee somehow started. Catastrophic failure.", evt.id, evt.priority, originatingOfflineServiceIt->second->serviceId(), originatingOfflineServiceIt->second->implementationName(), originatingOfflineServiceIt->second->getServiceState(), serviceIt->second->serviceId(), serviceIt->second->implementationName(), serviceIt->second->getServiceState());
                        std::terminate();
                    }
                }

                INTERNAL_DEBUG("ContinuableDependencyOfflineEvent {} {} {}:{} state {} dependee {}:{} state {} dependee DONE", evt.id, evt.priority, originatingOfflineServiceIt->second->serviceId(), originatingOfflineServiceIt->second->implementationName(), originatingOfflineServiceIt->second->getServiceState(), serviceIt->second->serviceId(), serviceIt->second->implementationName(), serviceIt->second->getServiceState());
                serviceIt->second->finishDependencyOffline(originatingOfflineServiceIt->second.get(), origEvt->dependencyIterators);

                if(originatingOfflineServiceIt->second->getDependees().empty()) {
                    INTERNAL_DEBUG("originatingOfflineService found waiting service {} {}", origEvt->originatingOfflineServiceId, originatingOfflineServiceIt->second->getServiceState());
                    // Service needs to be stopped to complete the sequence
                    _eventQueue.pushPrioritisedEvent<StopServiceEvent>(origEvt->originatingOfflineServiceId, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, evt.priority), origEvt->originatingOfflineServiceId, origEvt->removeOriginatingOfflineServiceAfterStop);
                    finishWaitingService(origEvt->originatingOfflineServiceId, DependencyOfflineEvent::TYPE, DependencyOfflineEvent::NAME);
                }
            }

            handleEventCompletion(*origEvt);
        } else {
            ICHOR_EMERGENCY_LOG2(_logger, "{}", genIt->second.event->get_name());
            ICHOR_EMERGENCY_LOG1(_logger, "Something went wrong, perhaps your coroutine uses the internal StartBehaviour as a return type. Otherwise, please file a bug in Ichor.");
            std::terminate();
        }

        if(!erasedScopedEntries) {
            _scopedGenerators.erase(genIt);
        }
    }
}
