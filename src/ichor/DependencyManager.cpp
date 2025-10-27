#include <thread>
#include <ichor/DependencyManager.h>
#include <ichor/ServiceExecutionScope.h>
#include <ichor/CommunicationChannel.h>
#include <ichor/Filter.h>
#include <ichor/stl/Any.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/dependency_management/InternalService.h>
#include <ichor/dependency_management/InternalServiceLifecycleManager.h>
#include <ichor/dependency_management/IServiceInterestedLifecycleManager.h>
#include <fmt/format.h>

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
#include <processthreadsapi.h>
#include <fmt/xchar.h>
#endif

constinit std::atomic<uint64_t> Ichor::DependencyManager::_managerIdCounter = 0;
#ifdef ICHOR_ENABLE_INTERNAL_STL_DEBUGGING
std::atomic<uint64_t> Ichor::v1::_rfpCounter = 0;
#endif
thread_local Ichor::unordered_set<Ichor::ServiceIdType, Ichor::ServiceIdHash> Ichor::Detail::emptyDependencies{}; // TODO modify ankerl's implementation to have enough constexpr functions to have this be constinit as well.

constinit thread_local Ichor::DependencyManager *Ichor::Detail::_local_dm = nullptr;

#ifdef ICHOR_USE_BACKWARD
#include <backward/backward.h>

namespace backward {
    backward::SignalHandling sh;
}
#endif

Ichor::DependencyManager::DependencyManager(IEventQueue *eventQueue) : _eventQueue(eventQueue) {
    auto dmlm = std::make_unique<Detail::InternalServiceLifecycleManager<DependencyManager>>(this);
    _services.emplace(dmlm->serviceId(), std::move(dmlm));
}

void Ichor::DependencyManager::start() {
    ICHOR_LOG_DEBUG(_logger, "starting dm {}", _id);

    _quitEventReceived = false;

    if(Detail::_local_dm != nullptr) [[unlikely]] {
        ICHOR_EMERGENCY_LOG1(_logger, "This thread already has a manager");
        std::terminate();
    }

    Ichor::Detail::_local_dm = this;
    _started.store(true, std::memory_order_release);

    ICHOR_LOG_TRACE(_logger, "depman {} has {} events", _id, _eventQueue->size());

#if defined(__APPLE__)
    pthread_setname_np(fmt::format("DepMan#{}", _id).c_str());
#endif
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
    SetThreadDescription(GetCurrentThread(), fmt::format(L"DepMan#{}", _id).c_str());
#endif
#if defined(__linux__) || defined(__CYGWIN__)
    pthread_setname_np(pthread_self(), fmt::format("DepMan#{}", _id).c_str());
#endif
}

void Ichor::DependencyManager::processEvent(std::unique_ptr<Event> &uniqueEvt) {
    if(_quitDone) [[unlikely]] {
        return;
    }
    // when this event needs to be stored in _scopedEvents, we move uniqueEvt and then re-assign evt.
    Event *evt = uniqueEvt.get();
    if(_logger != nullptr && _logger->getLogLevel() == LogLevel::LOG_TRACE) {
        auto const svcIt = _services.find(evt->originatingService);
        std::string_view svcName = "UNKNOWN";
        if(svcIt != _services.end()) {
            svcName = svcIt->second->implementationName();
        }
        ICHOR_LOG_TRACE(_logger, "depMgr {} evt id {} type {} has {} prio from {}", getId(), evt->id, evt->get_name(), evt->priority, svcName);
    }

    bool allowProcessing = true;
    uint64_t handlerAmount = 1; // for the non-default case below, the DepMan handles the event
    std::vector<EventInterceptInfo> allEventInterceptorsCopy{};
    std::vector<EventInterceptInfo> eventInterceptorsCopy{};
    auto evtType = evt->get_type();

    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if(evtType == 0) [[unlikely]] {
            ICHOR_EMERGENCY_LOG2(_logger, "evt {} has a type of 0, did you forget to override the get_id function?\n", evt->get_name());
            std::terminate();
        }
    }

    {
        auto const interceptorsForAllEvents = _eventInterceptors.find(0);
        auto const interceptorsForEvent = _eventInterceptors.find(evtType);

        if (interceptorsForAllEvents != end(_eventInterceptors)) {
            // Make copy because the vector can be modified in the preIntercept() call.
            allEventInterceptorsCopy = interceptorsForAllEvents->second;
            for (EventInterceptInfo const &info: allEventInterceptorsCopy) {
                ServiceExecutionScope serviceScope{info.listeningServiceId};
                if (!info.preIntercept(*evt)) {
                    allowProcessing = false;
                }
            }
        }

        if (interceptorsForEvent != end(_eventInterceptors)) {
            // Make copy because the vector can be modified in the preIntercept() call.
            eventInterceptorsCopy = interceptorsForEvent->second;
            for (EventInterceptInfo const &info: eventInterceptorsCopy) {
                ServiceExecutionScope serviceScope{info.listeningServiceId};
                if (!info.preIntercept(*evt)) {
                    allowProcessing = false;
                }
            }
        }
    }

    if (allowProcessing) {
        switch (evtType) {
            case DependencyOnlineEvent::TYPE: {
                auto *depOnlineEvt = static_cast<DependencyOnlineEvent *>(evt);
                auto const managerIt = _services.find(depOnlineEvt->originatingService);

                if (managerIt == end(_services)) [[unlikely]] {
                    INTERNAL_DEBUG("DependencyOnlineEvent not found {}", evt->id);
                    break;
                }

                auto &manager = managerIt->second;

                INTERNAL_DEBUG("DependencyOnlineEvent {} {} {}:{}", evt->id, evt->priority, manager->serviceId(), manager->implementationName());

                if (!manager->setInjected()) {
                    INTERNAL_DEBUG("Couldn't set injected for {} {} {}", manager->serviceId(), manager->implementationName(), manager->getServiceState());
                    break;
                }

                finishWaitingService(depOnlineEvt->originatingService, DependencyOnlineEvent::TYPE, DependencyOnlineEvent::NAME);

                if(!manager->getInterfaces().empty()) {
                    auto const filterProp = manager->getProperties().find("Filter");
                    const Filter *filter = nullptr;
                    if (filterProp != cend(manager->getProperties())) {
                        filter = Ichor::v1::any_cast<Filter *const>(&filterProp->second);
                    }

                    for (auto const &[serviceId, possibleDependentLifecycleManager] : _services) {
                        if (serviceId == depOnlineEvt->originatingService || (filter != nullptr && !filter->compareTo(*possibleDependentLifecycleManager))) {
                            INTERNAL_DEBUG("DependencyOnlineEvent {} {}:{} interested service is {}:{} skipping {} {}", evt->id, manager->serviceId(), manager->implementationName(), serviceId, possibleDependentLifecycleManager->implementationName(), serviceId == depOnlineEvt->originatingService, filter != nullptr);
                            continue;
                        }

                        ServiceExecutionScope serviceScope{serviceId};
                        auto startBehaviour = possibleDependentLifecycleManager->dependencyOnline(manager.get());

                        INTERNAL_DEBUG("DependencyOnlineEvent {} {}:{} interested service is {}:{} startBehaviour {}", evt->id, manager->serviceId(), manager->implementationName(), serviceId, possibleDependentLifecycleManager->implementationName(), startBehaviour);

                        if(startBehaviour == StartBehaviour::DONE) {
                            continue;
                        }

                        auto gen = possibleDependentLifecycleManager->startAfterDependencyOnline();
                        gen.set_priority(std::min(possibleDependentLifecycleManager->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY));
                        auto it = gen.begin();

                        INTERNAL_DEBUG("DependencyOnlineEvent {} {}:{} interested service is {}:{} {} {}", evt->id, manager->serviceId(), manager->implementationName(), serviceId, possibleDependentLifecycleManager->implementationName(), it.get_promise_id(), it.get_finished());

                        if(!it.get_finished()) {
                            if constexpr (DO_INTERNAL_DEBUG) {
                                if (!it.get_has_suspended()) [[unlikely]] {
                                    std::terminate();
                                }
                            }
                            gen.set_service_id_stack(std::move(serviceScope.getServiceIdStack()));
                            _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)));
                            // create new event that will be inserted upon finish of coroutine in ContinuableStartEvent
                            _scopedEvents.emplace(it.get_promise_id(), v1::make_reference_counted<DependencyOnlineEvent>(_eventQueue->getNextEventId(), serviceId, std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt->priority)));
                        } else if(it.get_value() == StartBehaviour::STARTED) {
                            _eventQueue->pushPrioritisedEvent<DependencyOnlineEvent>(serviceId, std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt->priority));
                        }
                    }
                }

                // If we're quitting, ensure newly started services are also stopped
                if(_quitEventReceived) {
                    _eventQueue->pushPrioritisedEvent<StopServiceEvent>(manager->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt->priority), manager->serviceId(), true);
                }
            }
                break;
            case DependencyOfflineEvent::TYPE: {
                auto *depOfflineEvt = static_cast<DependencyOfflineEvent *>(evt);
                auto const managerIt = _services.find(depOfflineEvt->originatingService);

                if (managerIt == end(_services)) [[unlikely]] {
                    break;
                }

                auto &manager = managerIt->second;
                INTERNAL_DEBUG("DependencyOfflineEvent {} {} {}:{} {} {}", evt->id, evt->priority, evt->originatingService, manager->implementationName(), manager->getServiceState(), depOfflineEvt->removeOriginatingServiceAfterStop);

                if (!manager->setUninjected()) {
                    // DependencyOfflineEvent already started or processed
                    INTERNAL_DEBUG("Couldn't set uninjected for {} {} {}", manager->serviceId(), manager->implementationName(), manager->getServiceState());
                    break;
                }

                // copy dependees as it will be modified during this loop
                auto dependees = manager->getDependees();
                bool allDependeesFinished{true};
                for (auto &serviceId : dependees) {
                    auto const depIt = _services.find(serviceId);

                    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                        if (depIt == _services.end()) [[unlikely]] {
                            ICHOR_EMERGENCY_LOG2(_logger, "Couldn't find dependendee {}.", serviceId);
                            std::terminate();
                        }
                    }

                    auto depIts = depIt->second->interestedInDependencyGoingOffline(manager.get());

                    if(depIts.empty()) {
                        continue;
                    }

                    ServiceExecutionScope serviceScope{serviceId};
                    auto gen = depIt->second->dependencyOffline(manager.get(), std::move(depIts));
                    gen.set_priority(std::min(depIt->second->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY));
                    auto it = gen.begin();

                    if(!it.get_finished()) {
                        if constexpr (DO_INTERNAL_DEBUG) {
                            if (!it.get_has_suspended()) [[unlikely]] {
                                INTERNAL_DEBUG("{}", it.get_promise_id());
                                std::terminate();
                            }
                        }
                        allDependeesFinished = false;
                        gen.set_service_id_stack(std::move(serviceScope.getServiceIdStack()));
                        _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)));
                        // create new event that will be inserted upon finish of coroutine in ContinuableStartEvent
                        _scopedEvents.emplace(it.get_promise_id(), v1::make_reference_counted<ContinuableDependencyOfflineEvent>(_eventQueue->getNextEventId(), serviceId, INTERNAL_DEPENDENCY_EVENT_PRIORITY, depOfflineEvt->originatingService, depOfflineEvt->removeOriginatingServiceAfterStop));
                        continue;
                    }

                    if constexpr (DO_INTERNAL_DEBUG) {
                        if (it.get_has_suspended()) [[unlikely]] {
                            INTERNAL_DEBUG("{}", it.get_promise_id());
                            std::terminate();
                        }
                    }

                    if(it.get_value() == StartBehaviour::STOPPED) {
                        INTERNAL_DEBUG("DependencyOfflineEvent {} {} {}:{} {} service {} stopped", evt->id, evt->priority, evt->originatingService, manager->implementationName(), manager->getServiceState(), serviceId);
                        clearServiceRegistrations(allEventInterceptorsCopy, eventInterceptorsCopy, serviceId);
                        finishWaitingService(serviceId, StopServiceEvent::TYPE, StopServiceEvent::NAME);
                        _eventQueue->pushPrioritisedEvent<DependencyOfflineEvent>(serviceId, evt->priority, false);
                    }
                }
                if(allDependeesFinished) {
                    finishWaitingService(depOfflineEvt->originatingService, DependencyOfflineEvent::TYPE, DependencyOfflineEvent::NAME);
                    _eventQueue->pushPrioritisedEvent<StopServiceEvent>(depOfflineEvt->originatingService, std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt->priority), depOfflineEvt->originatingService, depOfflineEvt->removeOriginatingServiceAfterStop);
                }
            }
                break;
            case DependencyRequestEvent::TYPE: {
                auto *depReqEvt = static_cast<DependencyRequestEvent *>(evt);
                INTERNAL_DEBUG("DependencyRequestEvent {} {} {} {}", evt->id, evt->priority, evt->originatingService, depReqEvt->dependency.getInterfaceName());

                auto const trackers = _dependencyRequestTrackers.find(depReqEvt->dependency.interfaceNameHash);
                if (trackers == end(_dependencyRequestTrackers)) {
                    break;
                }

                v1::ReferenceCountedPointer<Event> refEvt;
                for (DependencyTrackerInfo const &info : trackers->second) {
                    ServiceExecutionScope serviceScope{info.svcId}; // TODO check if this should be evt originating service
                    auto gen = info.trackFunc(*depReqEvt);
                    gen.set_priority(evt->priority);
                    auto it = gen.begin();

                    if(!it.get_finished()) {
                        gen.set_service_id_stack(std::move(serviceScope.getServiceIdStack()));
                        _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<IchorBehaviour>>(std::move(gen)));
                        if(!refEvt.has_value()) {
                            refEvt = std::move(uniqueEvt);
                            evt = refEvt.get();
                        }
                        _scopedEvents.emplace(it.get_promise_id(), refEvt);
                    }
                }
            }
                break;
            case QuitEvent::TYPE: {
                INTERNAL_DEBUG("QuitEvent {} {} {}", evt->id, evt->priority, evt->originatingService);
                auto *_quitEvt = static_cast<QuitEvent *>(evt);

                _quitEventReceived = true;

                uint64_t lowest_priority{evt->priority};

                for (auto const &[key, possibleManager]: _services) {
                    if (possibleManager->getServiceState() == ServiceState::ACTIVE) {
                        if(possibleManager->isInternalManager()) {
                            continue;
                        }

                        auto &dependees = possibleManager->getDependees();

                        auto priority = std::max(possibleManager->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY);

                        if(priority > lowest_priority) {
                            lowest_priority = priority;
                        }

                        for(auto const &serviceId : dependees) {
                            _eventQueue->pushPrioritisedEvent<StopServiceEvent>(_quitEvt->originatingService, priority, serviceId, true);
                        }

                        _eventQueue->pushPrioritisedEvent<StopServiceEvent>(_quitEvt->originatingService, priority, possibleManager->serviceId(), true);
                    }

                    if(possibleManager->getServiceState() != ServiceState::INSTALLED) {
                        if constexpr (DO_INTERNAL_DEBUG) {
                            INTERNAL_DEBUG("Service {}:{} {}", key, possibleManager->implementationName(), possibleManager->getServiceState());
                            for (auto dep: possibleManager->getDependees()) {
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

                checkIfCanQuit(allEventInterceptorsCopy, eventInterceptorsCopy);
            }
                break;
            case InsertServiceEvent::TYPE: {
                auto *insertServiceEvt = static_cast<InsertServiceEvent *>(evt);
                INTERNAL_DEBUG("InsertServiceEvent {} {} {}:{}", evt->id, evt->priority, evt->originatingService, insertServiceEvt->mgr->implementationName());
                auto svcIt = _services.emplace(insertServiceEvt->originatingService, std::move(insertServiceEvt->mgr));
                auto &cmpMgr = svcIt.first->second;

                // If a service requests IService, we interpret it to mean a reference to itself, not just all services in existence.
                Detail::IServiceInterestedLifecycleManager selfMgr{cmpMgr->getIService()};
                {
                    ServiceExecutionScope serviceScope{evt->originatingService};
                    auto startBehaviour = cmpMgr->dependencyOnline(&selfMgr);

                    if(startBehaviour == StartBehaviour::STARTED) {
                        auto gen = cmpMgr->startAfterDependencyOnline();
                        gen.set_priority(std::min(cmpMgr->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY));
                        auto it = gen.begin();

                        if(!it.get_finished()) {
                            gen.set_service_id_stack(std::move(serviceScope.getServiceIdStack()));
                            _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)));
                            // create new event that will be inserted upon finish of coroutine in ContinuableStartEvent
                            _scopedEvents.emplace(it.get_promise_id(), v1::make_reference_counted<DependencyOnlineEvent>(_eventQueue->getNextEventId(), cmpMgr->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt->priority)));
                        } else if(it.get_value() == StartBehaviour::STARTED) {
                            _eventQueue->pushPrioritisedEvent<DependencyOnlineEvent>(cmpMgr->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt->priority));
                        }
                    }
                }

                if(cmpMgr->getDependencyRegistry() == nullptr || cmpMgr->getDependencyRegistry()->empty()) {
                    break;
                }

                // loop over all services, check if cmpMgr is interested in the active ones and inject them if so
                for (auto &[key, mgr] : _services) {
                    if (mgr->getServiceState() != ServiceState::ACTIVE || mgr->getInterfaces().empty()) {
                        INTERNAL_DEBUG("InsertServiceEvent {} {}:{} interested service is {}:{} skipping {}", evt->id, cmpMgr->serviceId(), cmpMgr->implementationName(), key, mgr->implementationName(), mgr->getServiceState());
                        continue;
                    }

                    auto const filterProp = mgr->getProperties().find("Filter");
                    const Filter *filter = nullptr;
                    if (filterProp != cend(mgr->getProperties())) {
                        filter = Ichor::v1::any_cast<Filter * const>(&filterProp->second);
                    }

                    if (filter != nullptr && !filter->compareTo(*cmpMgr.get())) {
                        continue;
                    }

                    ServiceExecutionScope serviceScope{evt->originatingService};
                    auto startBehaviour = cmpMgr->dependencyOnline(mgr.get());

                    INTERNAL_DEBUG("InsertServiceEvent {} {}:{} interested service is {}:{} startBehaviour {}", evt->id, cmpMgr->serviceId(), cmpMgr->implementationName(), key, mgr->implementationName(), startBehaviour);

                    if(startBehaviour == StartBehaviour::DONE) {
                        continue;
                    }

                    auto gen = cmpMgr->startAfterDependencyOnline();
                    gen.set_priority(std::min(cmpMgr->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY));
                    auto it = gen.begin();

                    if(!it.get_finished()) {
                        gen.set_service_id_stack(std::move(serviceScope.getServiceIdStack()));
                        _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)));
                        // create new event that will be inserted upon finish of coroutine in ContinuableStartEvent
                        _scopedEvents.emplace(it.get_promise_id(), v1::make_reference_counted<DependencyOnlineEvent>(_eventQueue->getNextEventId(), cmpMgr->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt->priority)));
                    } else if(it.get_value() == StartBehaviour::STARTED) {
                        _eventQueue->pushPrioritisedEvent<DependencyOnlineEvent>(cmpMgr->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt->priority));
                    }
                }
            }
                break;
            case StopServiceEvent::TYPE: {
                auto *stopServiceEvt = static_cast<StopServiceEvent *>(evt);
                ILifecycleManager *toStopService;

                {
                    auto const toStopServiceIt = _services.find(stopServiceEvt->serviceId);

                    if (toStopServiceIt == end(_services)) [[unlikely]] {
                        // ICHOR_LOG_ERROR(_logger, "Couldn't stop service {}, missing from known services", stopServiceEvt->serviceId);
                        break;
                    }

                    toStopService = toStopServiceIt->second.get();
                }

                INTERNAL_DEBUG("StopServiceEvent {} {} {}:{} state {} dependees {} removeAfter {}", evt->id, evt->priority, stopServiceEvt->serviceId, toStopService->implementationName(), toStopService->getServiceState(), toStopService->getDependees().size(), stopServiceEvt->removeAfter);

                // already stopped
                if(toStopService->getServiceState() == ServiceState::INSTALLED) {
                    INTERNAL_DEBUG("already stopped");
                    break;
                }
                if(toStopService->getServiceState() == ServiceState::STOPPING) {
                    INTERNAL_DEBUG("already stopping");
                    break;
                }

                auto &dependencies = toStopService->getDependencies();
                auto &dependees = toStopService->getDependees();

                auto const depWaiterIt = _dependencyWaiters.find(stopServiceEvt->serviceId);
                if(depWaiterIt != _dependencyWaiters.end()) {
                    if(!dependees.empty() || toStopService->getServiceState() != ServiceState::UNINJECTING) {
                        INTERNAL_DEBUG("existing dependency offline waiter {} {} {}", dependees.size(), toStopService->getServiceState(), depWaiterIt->second.events.size());
                        if constexpr (DO_INTERNAL_DEBUG) {
                            for (auto serviceId : dependees) {
                                INTERNAL_DEBUG("dependee: {}", serviceId);
                            }
                        }
                        break;
                    }

                    INTERNAL_DEBUG("waitForService done {} {} {}", stopServiceEvt->serviceId, DependencyOfflineEvent::NAME, depWaiterIt->second.events.size());

                    finishWaitingService(stopServiceEvt->serviceId, DependencyOfflineEvent::TYPE, DependencyOfflineEvent::NAME);
                }

                // coroutine needs to finish before we can stop the service
                if(existingCoroutineFor(stopServiceEvt->serviceId)) {
                    _pendingStops.emplace(stopServiceEvt->serviceId, WaitingStopService{stopServiceEvt->originatingService, stopServiceEvt->priority, stopServiceEvt->removeAfter});
                    INTERNAL_DEBUG("existing scoped event");
                    break;
                }

                if constexpr (DO_INTERNAL_DEBUG) {
                    for (auto serviceId : dependencies) {
                        INTERNAL_DEBUG("dependency: {}", serviceId);
                    }

                    for (auto serviceId : dependees) {
                        INTERNAL_DEBUG("dependee: {}", serviceId);
                    }
                }

                // If all services depending on this one have removed this one and we've had a DependencyOfflineEvent
                if (dependees.empty() && toStopService->getServiceState() == ServiceState::UNINJECTING) {
                    if(_logger != nullptr && stopServiceEvt->serviceId == _logger->getFrameworkServiceId()) {
                        _logger = nullptr;
                    }

                    ServiceExecutionScope serviceScope{stopServiceEvt->serviceId};
                    auto gen = toStopService->stop();
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
                        gen.set_service_id_stack(std::move(serviceScope.getServiceIdStack()));
                        _scopedGenerators.emplace(prom_id, std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)));
                        auto scopedIt = _scopedEvents.emplace(prom_id, std::move(uniqueEvt));
                        evt = scopedIt.first->second.get();
                        break;
                    }

                    if constexpr (DO_INTERNAL_DEBUG || DO_INTERNAL_COROUTINE_DEBUG) {
                        if (it.get_has_suspended()) [[unlikely]] {
                            INTERNAL_DEBUG("{}", it.get_promise_id());
                            std::terminate();
                        }
                    }

                    INTERNAL_DEBUG("service->stop() {}:{} state {}", stopServiceEvt->serviceId, toStopService->implementationName(), toStopService->getServiceState());
                    if (toStopService->getServiceState() != ServiceState::INSTALLED) [[unlikely]] {
                        break;
                    }

                    for (auto serviceId : dependencies) {
                        auto const depIt = _services.find(serviceId);

                        if (depIt == _services.end()) {
                            continue;
                        }

                        depIt->second->getDependees().erase(stopServiceEvt->serviceId);
                    }
                    dependencies.clear();
                    clearServiceRegistrations(allEventInterceptorsCopy, eventInterceptorsCopy, stopServiceEvt->serviceId);

                    finishWaitingService(stopServiceEvt->serviceId, StopServiceEvent::TYPE, StopServiceEvent::NAME);

                    INTERNAL_DEBUG("service->stop() {}:{} removeAfter {}", stopServiceEvt->serviceId, toStopService->implementationName(), stopServiceEvt->removeAfter);
                    if(stopServiceEvt->removeAfter) {
                        removeInternalService(allEventInterceptorsCopy, eventInterceptorsCopy, stopServiceEvt->serviceId);
                    }

                    checkIfCanQuit(allEventInterceptorsCopy, eventInterceptorsCopy);
                } else {
                    // Trigger dependency offline handling once; StopServiceEvent will be (re)scheduled
                    // after all DependencyOfflineEvents have completed to avoid event storms.
                    _eventQueue->pushPrioritisedEvent<DependencyOfflineEvent>(stopServiceEvt->serviceId, evt->priority, stopServiceEvt->removeAfter);
                }
            }
                break;
            case StartServiceEvent::TYPE: {
                auto *startServiceEvt = static_cast<StartServiceEvent *>(evt);

                auto const toStartServiceIt = _services.find(startServiceEvt->serviceId);

                if (toStartServiceIt == end(_services)) [[unlikely]] {
                    INTERNAL_DEBUG( "Couldn't start service {}, missing from known services", startServiceEvt->serviceId);

                    if constexpr (DO_INTERNAL_DEBUG) {
                        for(auto const &[svcId, svc] : _services) {
                            INTERNAL_DEBUG( "service {}:{}", svcId, svc->implementationName());
                        }
                    }
                    break;
                }

                auto *toStartService = toStartServiceIt->second.get();
                if (toStartService->getServiceState() == ServiceState::ACTIVE) {
                    INTERNAL_DEBUG("StartServiceEvent service {}:{} already started", toStartService->serviceId(), toStartService->implementationName());
                    break;
                }
                if (toStartService->getServiceState() == ServiceState::STARTING || toStartService->getServiceState() == ServiceState::INJECTING) {
                    INTERNAL_DEBUG("StartServiceEvent service {}:{} already starting", toStartService->serviceId(), toStartService->implementationName());
                    break;
                }

                INTERNAL_DEBUG("StartServiceEvent service {} {} {}:{}", evt->id, evt->priority, toStartService->serviceId(), toStartService->implementationName());
                ServiceExecutionScope serviceScope{startServiceEvt->serviceId};
                auto gen = toStartService->start();
                gen.set_priority(std::min(toStartService->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY));
                auto it = gen.begin();

                if (!it.get_finished()) {
                    if constexpr (DO_INTERNAL_DEBUG || DO_INTERNAL_COROUTINE_DEBUG) {
                        if (!it.get_has_suspended()) [[unlikely]] {
                            std::terminate();
                        }
                    }
                    INTERNAL_DEBUG("StartServiceEvent contains {}:{} {} {} {}", toStartService->serviceId(), toStartService->implementationName(), it.get_promise_id(), _scopedGenerators.contains(it.get_promise_id()),
                                   _scopedGenerators.size() + 1);
                    gen.set_service_id_stack(std::move(serviceScope.getServiceIdStack()));
                    _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)));
                    auto scopedIt = _scopedEvents.emplace(it.get_promise_id(), std::move(uniqueEvt));
                    evt = scopedIt.first->second.get();
                    break;
                }

                if constexpr (DO_INTERNAL_DEBUG || DO_INTERNAL_COROUTINE_DEBUG) {
                    if (it.get_has_suspended()) [[unlikely]] {
                        INTERNAL_DEBUG("{}", it.get_promise_id());
                        std::terminate();
                    }
                }

                if(toStartService->getServiceState() == ServiceState::INSTALLED) [[unlikely]] {
                    INTERNAL_DEBUG("StartServiceEvent service INSTALLED {}:{} {} {}", toStartService->serviceId(), toStartService->implementationName(), it.get_promise_id(), it.get_finished());
                    break;
                }

                INTERNAL_DEBUG("StartServiceEvent finished {}:{} {}", toStartService->serviceId(), toStartService->implementationName(), it.get_promise_id());

                _eventQueue->pushPrioritisedEvent<DependencyOnlineEvent>(toStartService->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt->priority));
                // If we're quitting, ensure newly started services are also stopped
                if(_quitEventReceived) {
                    _eventQueue->pushPrioritisedEvent<StopServiceEvent>(toStartService->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt->priority), toStartService->serviceId(), true);
                }
            }
                break;
            case RemoveServiceEvent::TYPE: {
                auto *removeServiceEvt = static_cast<RemoveServiceEvent *>(evt);

                auto const toRemoveServiceIt = _services.find(removeServiceEvt->serviceId);

                if (toRemoveServiceIt == end(_services)) [[unlikely]] {
                    ICHOR_LOG_ERROR(_logger, "Couldn't remove service {}, missing from known services", removeServiceEvt->serviceId);
                    break;
                }

                auto *toRemoveService = toRemoveServiceIt->second.get();
                INTERNAL_DEBUG("RemoveServiceEvent {} {} {}:{} {}", evt->id, evt->priority, toRemoveService->serviceId(), toRemoveService->implementationName(), toRemoveService->getServiceState());

                if(toRemoveService->getServiceState() != ServiceState::INSTALLED || !toRemoveService->getDependees().empty() || !toRemoveService->getDependencies().empty()) {
                    INTERNAL_DEBUG("Couldn't remove service {}, not stopped first {} {}", removeServiceEvt->serviceId, toRemoveService->getDependees().size(), toRemoveService->getDependencies().size());
                    ICHOR_LOG_ERROR(_logger, "Couldn't remove service {}, not stopped first", removeServiceEvt->serviceId);
                    break;
                }

                removeInternalService(allEventInterceptorsCopy, eventInterceptorsCopy, toRemoveServiceIt->first);
            }
                break;
            case RemoveEventHandlerEvent::TYPE: {
                INTERNAL_DEBUG("RemoveEventHandlerEvent {} {}", evt->id, evt->priority);
                auto *removeEventHandlerEvt = static_cast<RemoveEventHandlerEvent *>(evt);

                // key.id = service id, key.type == event id
                auto const existingHandlers = _eventCallbacks.find(removeEventHandlerEvt->key.type);
                if (existingHandlers != end(_eventCallbacks)) [[likely]] {
                    std::erase_if(existingHandlers->second, [removeEventHandlerEvt](const EventCallbackInfo &info) noexcept {
                        return info.listeningServiceId == ServiceIdType{removeEventHandlerEvt->key.id};
                    });
                }
            }
                break;
            case RemoveEventInterceptorEvent::TYPE: {
                INTERNAL_DEBUG("RemoveEventInterceptorEvent {} {}", evt->id, evt->priority);
                auto *removeEventHandlerEvt = static_cast<RemoveEventInterceptorEvent *>(evt);

                // key.id = service id, key.type == event id
                auto const existingHandlers = _eventInterceptors.find(removeEventHandlerEvt->eventType);
                if (existingHandlers != end(_eventInterceptors)) [[likely]] {
                    std::erase_if(existingHandlers->second, [removeEventHandlerEvt](const EventInterceptInfo &info) noexcept {
                        return info.interceptorId == removeEventHandlerEvt->interceptorId;
                    });
                }
            }
                break;
            case AddTrackerEvent::TYPE: {
                INTERNAL_DEBUG("AddTrackerEvent {} {}", evt->id, evt->priority);
                auto *addTrackerEvt = static_cast<AddTrackerEvent *>(evt);

                if(auto const trackers = _dependencyRequestTrackers.find(addTrackerEvt->interfaceNameHash); trackers != _dependencyRequestTrackers.end()) {
                    for(auto &tracker : trackers->second) {
                        if(tracker.svcId != addTrackerEvt->originatingService) {
                            continue;
                        }

                        std::vector<v1::ReferenceCountedPointer<DependencyRequestEvent>> requests{};
                        for(auto const &[key, mgr] : _services) {
                            auto const *depRegistry = mgr->getDependencyRegistry();
//                            ICHOR_LOG_ERROR(_logger, "register svcId {} dm {}", mgr->serviceId(), _id);

                            // only DependencyLifecycleManager has a non-nullptr value. Other Lifecyclemanagers return nullptr because they don't request dependencies.
                            if(depRegistry == nullptr) {
                                continue;
                            }

                            for (auto const &[interfaceHash, registration] : depRegistry->_registrations) {
                                if(interfaceHash == addTrackerEvt->interfaceNameHash) {
                                    auto const &props = std::get<tl::optional<Properties>>(registration);
                                    requests.emplace_back(0u, mgr->serviceId(), std::min(mgr->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY), std::get<Dependency>(registration), props.has_value() ? &props.value() : tl::optional<Properties const *>{});
                                }
                            }
                        }

                        for(auto& request : requests) {
                            ServiceExecutionScope serviceScope{evt->originatingService};
                            auto gen = tracker.trackFunc(*request);
                            gen.set_priority(evt->priority);
                            auto it = gen.begin();

                            if(!it.get_finished()) {
                                gen.set_service_id_stack(std::move(serviceScope.getServiceIdStack()));
                                _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<IchorBehaviour>>(std::move(gen)));
                                _scopedEvents.emplace(it.get_promise_id(), std::move(request));
                            }
                        }

                        break;
                    }
                }
            }
                break;
            case RemoveTrackerEvent::TYPE: {
                INTERNAL_DEBUG("RemoveTrackerEvent {} {}", evt->id, evt->priority);
                auto *removeTrackerEvt = static_cast<RemoveTrackerEvent *>(evt);

                if(auto const trackers = _dependencyRequestTrackers.find(removeTrackerEvt->interfaceNameHash); trackers != _dependencyRequestTrackers.end()) {
                    std::erase_if(trackers->second, [&removeTrackerEvt](DependencyTrackerInfo const &info) {
                        return info.svcId == removeTrackerEvt->originatingService;
                    });
                    if(trackers->second.empty()) {
                        _dependencyRequestTrackers.erase(trackers);
                    }
                }
            }
                break;
            case ContinuableEvent::TYPE: {
                auto *continuableEvt = static_cast<ContinuableEvent *>(evt);
                INTERNAL_DEBUG("ContinuableEvent {} {} {}", continuableEvt->promiseId, evt->id, evt->priority);
                auto const genIt = _scopedGenerators.find(continuableEvt->promiseId);

                if (genIt != _scopedGenerators.end()) {
                    INTERNAL_DEBUG("ContinuableEvent2 {}", genIt->second->done());

                    if (!genIt->second->done()) {
                        auto it = genIt->second->begin_interface();
                        INTERNAL_DEBUG("ContinuableEvent it {} {} {}", it->get_finished(), it->get_op_state(), it->get_promise_state());

                        if (!it->get_finished() && it->get_promise_state() != state::value_not_ready_consumer_active) {
                            if constexpr (DO_INTERNAL_DEBUG || DO_INTERNAL_COROUTINE_DEBUG) {
                                if (!it->get_has_suspended()) [[unlikely]] {
                                    INTERNAL_DEBUG("{}", it->get_promise_id());
                                    std::terminate();
                                }
                            }
//                            INTERNAL_DEBUG("ContinuableEvent push event {}", continuableEvt->promiseId);
//                            _eventQueue->pushPrioritisedEvent<ContinuableEvent>(continuableEvt->originatingService, evt->priority, continuableEvt->promiseId);
                        }

                        if (it->get_finished()) {
                            INTERNAL_DEBUG("removed1 {} size {}", continuableEvt->promiseId, _scopedGenerators.size() - 1);
                            auto const origEventIt = _scopedEvents.find(continuableEvt->promiseId);

                            if (origEventIt != end(_scopedEvents)) [[unlikely]] {
                                INTERNAL_DEBUG("removed1 {} use_count {}", continuableEvt->promiseId, origEventIt->second.use_count());
                                if(origEventIt->second.use_count() == 1) {
                                    handleEventCompletion(*origEventIt->second);
                                }
                                _scopedEvents.erase(origEventIt);
                            }

                            for(auto const &id : genIt->second->get_service_id_stack()) {
                                INTERNAL_DEBUG("removed1 {} checking pending stop for {}", continuableEvt->promiseId, id.id);

                                auto pendingStopIt = _pendingStops.find(id.id);

                                if(pendingStopIt == _pendingStops.end()) {
                                    continue;
                                }

                                INTERNAL_DEBUG("removed1 {} re-triggering StopServiceEvent for {}", continuableEvt->promiseId, id.id);

                                _eventQueue->pushPrioritisedEvent<StopServiceEvent>(
                                    pendingStopIt->second.originatingServiceId, std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, pendingStopIt->second.priority), id.id, pendingStopIt->second.removeAfter);
                            }

                            _scopedGenerators.erase(genIt);
                        }
                    } else {
                        INTERNAL_DEBUG("removed2 {} size {}", continuableEvt->promiseId, _scopedGenerators.size() - 1);
                        auto const origEventIt = _scopedEvents.find(continuableEvt->promiseId);

                        if (origEventIt != end(_scopedEvents)) [[unlikely]] {
                            INTERNAL_DEBUG("removed2 {} use_count {}", continuableEvt->promiseId, origEventIt->second.use_count());
                            if(origEventIt->second.use_count() == 1) {
                                handleEventCompletion(*origEventIt->second);
                            }
                            _scopedEvents.erase(origEventIt);
                        }

                        for(auto const &id : genIt->second->get_service_id_stack()) {
                            INTERNAL_DEBUG("removed2 {} checking pending stop for {}", continuableEvt->promiseId, id.id);

                            auto pendingStopIt = _pendingStops.find(id.id);

                            if(pendingStopIt == _pendingStops.end()) {
                                continue;
                            }

                            INTERNAL_DEBUG("removed2 {} re-triggering StopServiceEvent for {}", continuableEvt->promiseId, id.id);

                            _eventQueue->pushPrioritisedEvent<StopServiceEvent>(
                                pendingStopIt->second.originatingServiceId, std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, pendingStopIt->second.priority), id.id, pendingStopIt->second.removeAfter);
                        }

                        _scopedGenerators.erase(genIt);
                    }
                    checkIfCanQuit(allEventInterceptorsCopy, eventInterceptorsCopy);
                }
            }
                break;
            case ContinuableStartEvent::TYPE: {
                auto *continuableEvt = static_cast<ContinuableStartEvent *>(evt);
                INTERNAL_DEBUG("ContinuableStartEvent {} {} {}", continuableEvt->promiseId, evt->id, evt->priority);
                auto const genIt = _scopedGenerators.find(continuableEvt->promiseId);

                if (genIt != _scopedGenerators.end()) {
                    INTERNAL_DEBUG("ContinuableStartEvent {}", genIt->second->done());

                    StartBehaviour it_ret;
                    if (!genIt->second->done()) {
                        auto it = genIt->second->begin_interface();
                        INTERNAL_DEBUG("ContinuableStartEvent it {} {} {}", it->get_finished(), it->get_op_state(), it->get_promise_state());

                        if (!it->get_finished()) {
                            if constexpr (DO_INTERNAL_DEBUG || DO_INTERNAL_COROUTINE_DEBUG) {
                                if (!it->get_has_suspended()) [[unlikely]] {
                                    INTERNAL_DEBUG("{}", it->get_promise_id());
                                    std::terminate();
                                }
                            }
                            break;
                        }

                        it_ret = static_cast<Detail::AsyncGeneratorBeginOperation<StartBehaviour>*>(it.get())->get_value();
                    } else {
                        it_ret = static_cast<AsyncGenerator<StartBehaviour>*>(genIt->second.get())->get_value();
                    }

                    INTERNAL_DEBUG("ContinuableStartEvent removed {} {} {}", it_ret, continuableEvt->promiseId, _scopedGenerators.size() - 1);
                    auto const origEvtIt = _scopedEvents.find(continuableEvt->promiseId);

                    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                        if (origEvtIt == end(_scopedEvents)) [[unlikely]] {
                            ICHOR_EMERGENCY_LOG2(_logger, "Couldn't find orig evt associated with promise {}", continuableEvt->promiseId);
                            std::terminate();
                        }
                    }

                    auto origEvtType = origEvtIt->second->get_type();
                    bool erasedScopedEntries{false};

                    if(origEvtType == StartServiceEvent::TYPE) {
                        auto const origEvt = static_cast<StartServiceEvent*>(origEvtIt->second.get());

                        INTERNAL_DEBUG("Finishing handling StartServiceEvent {} {} {} {}", origEvt->id, origEvt->priority, origEvt->originatingService, origEvt->serviceId);

                        _eventQueue->pushPrioritisedEvent<DependencyOnlineEvent>(origEvt->serviceId, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, evt->priority));
                        // If we're quitting, ensure newly started services are also stopped
                        if(_quitEventReceived) {
                            _eventQueue->pushPrioritisedEvent<StopServiceEvent>(origEvt->serviceId, std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt->priority), origEvt->serviceId, true);
                        }
                        handleEventCompletion(*origEvt);
                    } else if(origEvtType == StopServiceEvent::TYPE) {
                        auto origEvt = static_cast<StopServiceEvent *>(origEvtIt->second.get());

                        INTERNAL_DEBUG("Finishing handling StopServiceEvent {} {} {} {} {}", origEvt->id, origEvt->priority, origEvt->originatingService, origEvt->serviceId, origEvt->removeAfter);

                        finishWaitingService(origEvt->serviceId, StopServiceEvent::TYPE, StopServiceEvent::NAME);
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
                                for(auto dep : serviceIt->second->getDependees()) {
                                    INTERNAL_DEBUG("dep: {}", dep);
                                }
                                std::terminate();
                            }
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

                        clearServiceRegistrations(allEventInterceptorsCopy, eventInterceptorsCopy, origEvt->serviceId);
                        if(origEvt->removeAfter) {
                            removeInternalService(allEventInterceptorsCopy, eventInterceptorsCopy, origEvt->serviceId);
                        }

                        handleEventCompletion(*origEvt);

                        _scopedGenerators.erase(genIt);
                        _scopedEvents.erase(origEvtIt);
                        erasedScopedEntries = true;

                        checkIfCanQuit(allEventInterceptorsCopy, eventInterceptorsCopy);
                    } else if(origEvtType == DependencyOnlineEvent::TYPE) {
                        auto origEvt = static_cast<DependencyOnlineEvent *>(origEvtIt->second.get());

                        INTERNAL_DEBUG("Finishing handling DependencyOnlineEvent {} {} {}", origEvt->id, origEvt->priority, origEvt->originatingService);

                        if(it_ret == StartBehaviour::STARTED) {
                            // The dependee went offline during the async handling of the original
                            // DependencyOnlineEvent. Add a proper DependencyOfflineEvent to handle that.
                            // That is, originatingService points to the dependee, not the original service that went offline.
                            _eventQueue->pushPrioritisedEvent<DependencyOnlineEvent>(origEvt->originatingService, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, evt->priority));
                        }
                        handleEventCompletion(*origEvt);
                    } else if(origEvtType == ContinuableDependencyOfflineEvent::TYPE) {
                        auto origEvt = static_cast<ContinuableDependencyOfflineEvent *>(origEvtIt->second.get());

                        INTERNAL_DEBUG("Finishing handling ContinuableDependencyOfflineEvent {} {} {} {} {}", origEvt->id, origEvt->priority, origEvt->originatingService, origEvt->originatingOfflineServiceId, origEvt->removeOriginatingOfflineServiceAfterStop);

                        if(it_ret == StartBehaviour::STOPPED) {
                            if constexpr (DO_INTERNAL_DEBUG) {
                                [[maybe_unused]] auto const serviceIt = _services.find(origEvt->originatingService);
                                INTERNAL_DEBUG("it_ret stopped {} {}", origEvt->originatingService, serviceIt->second->getServiceState());
                            }
                            // The dependee of originatingOfflineServiceId went offline during the async handling of the original
                            // DependencyOfflineEvent. Add a proper DependencyOfflineEvent to handle that.
                            _eventQueue->pushPrioritisedEvent<DependencyOfflineEvent>(origEvt->originatingService, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, evt->priority), origEvt->removeOriginatingOfflineServiceAfterStop);
                            clearServiceRegistrations(allEventInterceptorsCopy, eventInterceptorsCopy, origEvt->originatingService);
                            finishWaitingService(origEvt->originatingService, StopServiceEvent::TYPE, StopServiceEvent::NAME);
                        }

                        // the originating event is actually from a dependee of the originatingOfflineServiceId
                        // We need to check if all its dependees are handled and if so, finish its offline waiter
                        auto const originatingOfflineServiceIt = _services.find(origEvt->originatingOfflineServiceId);
#ifdef ICHOR_ENABLE_INTERNAL_DEBUGGING
                        if(originatingOfflineServiceIt != _services.end()) {
                            auto const serviceIt = _services.find(origEvt->originatingOfflineServiceId);
                            auto &deps = originatingOfflineServiceIt->second->getDependees();
                            INTERNAL_DEBUG("originatingOfflineService {} {} {}", origEvt->originatingOfflineServiceId, serviceIt->second->getServiceState(), deps.size());
                            for(auto dep : deps) {
                                INTERNAL_DEBUG("dep: {}", dep);
                            }
                        }
#endif

                        if(originatingOfflineServiceIt != _services.end() && originatingOfflineServiceIt->second->getDependees().empty()) {
#ifdef ICHOR_ENABLE_INTERNAL_DEBUGGING
                            auto const serviceIt = _services.find(origEvt->originatingOfflineServiceId);
                            INTERNAL_DEBUG("originatingOfflineService found waiting service {} {}", origEvt->originatingOfflineServiceId, serviceIt->second->getServiceState());
#endif
                            // Service needs to be stopped to complete the sequence
                            _eventQueue->pushPrioritisedEvent<StopServiceEvent>(origEvt->originatingOfflineServiceId, std::min(INTERNAL_COROUTINE_EVENT_PRIORITY, evt->priority), origEvt->originatingOfflineServiceId, origEvt->removeOriginatingOfflineServiceAfterStop);
                        }

                        handleEventCompletion(*origEvt);
                    } else {
                        ICHOR_EMERGENCY_LOG2(_logger, "{}", origEvtIt->second->get_name());
                        ICHOR_EMERGENCY_LOG1(_logger, "Something went wrong, perhaps your coroutine uses the internal StartBehaviour as a return type. Otherwise, please file a bug.");
                        std::terminate();
                    }

                    if(!erasedScopedEntries) {
                        _scopedGenerators.erase(genIt);
                        _scopedEvents.erase(origEvtIt);
                    }
                }
            }
                break;

            case RunFunctionEvent::TYPE: {
                INTERNAL_DEBUG("RunFunctionEvent {} {}", evt->id, evt->priority);
                auto *runFunctionEvt = static_cast<RunFunctionEvent *>(evt);

                // Do not handle stale run function events
                if(runFunctionEvt->originatingService != ServiceIdType{0}) {
                    auto const requestingServiceIt = _services.find(runFunctionEvt->originatingService);
                    if(requestingServiceIt == end(_services)) {
                        INTERNAL_DEBUG("Service {} not found", runFunctionEvt->originatingService);
                        break;
                    }
                    if(requestingServiceIt->second->getServiceState() == ServiceState::INSTALLED) {
                        INTERNAL_DEBUG("Service {}:{} not active", runFunctionEvt->originatingService, requestingServiceIt->second->implementationName());
                        break;
                    }
                }

                ServiceExecutionScope serviceScope{runFunctionEvt->originatingService};
                runFunctionEvt->fun();
            }
                break;
            case RunFunctionEventAsync::TYPE: {
                INTERNAL_DEBUG("RunFunctionEventAsync {} {}", evt->id, evt->priority);
                auto *runFunctionEvt = static_cast<RunFunctionEventAsync *>(evt);

                // Do not handle stale run function events
                if(runFunctionEvt->originatingService != ServiceIdType{0}) {
                    auto const requestingServiceIt = _services.find(runFunctionEvt->originatingService);
                    if(requestingServiceIt == end(_services)) {
                        INTERNAL_DEBUG("Service {} not found", runFunctionEvt->originatingService);
                        break;
                    }
                    if(requestingServiceIt->second->getServiceState() == ServiceState::INSTALLED) {
                        INTERNAL_DEBUG("Service {}:{} not active", runFunctionEvt->originatingService, requestingServiceIt->second->implementationName());
                        break;
                    }
                }

                ServiceExecutionScope serviceScope{evt->originatingService};
                auto gen = runFunctionEvt->fun();
                gen.set_priority(evt->priority);
                auto it = gen.begin();
                INTERNAL_DEBUG("state: {} {} {} {} {}", gen.done(), it.get_finished(), it.get_op_state(), it.get_promise_state(), it.get_promise_id());

//                if (!it.get_finished() && it.get_promise_state() != state::value_not_ready_consumer_active) {
//                    _eventQueue->pushPrioritisedEvent<ContinuableEvent>(runFunctionEvt->originatingService, evt->priority, it.get_promise_id());
//                }

                if (!it.get_finished()) {
                    if constexpr (DO_INTERNAL_DEBUG) {
                        if (!it.get_has_suspended()) [[unlikely]] {
                            std::terminate();
                        }
                    }
                    INTERNAL_DEBUG("contains2 {} {} {}", it.get_promise_id(), _scopedGenerators.contains(it.get_promise_id()),
                                   _scopedGenerators.size() + 1);
                    gen.set_service_id_stack(std::move(serviceScope.getServiceIdStack()));
                    _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<IchorBehaviour>>(std::move(gen)));
                    auto scopedIt = _scopedEvents.emplace(it.get_promise_id(), std::move(uniqueEvt));
                    evt = scopedIt.first->second.get();
                } else {
                    if constexpr (DO_INTERNAL_DEBUG) {
                        if (it.get_has_suspended()) [[unlikely]] {
                            std::terminate();
                        }
                    }
                }
            }
                break;
            default: {
                INTERNAL_DEBUG("{} {} {}", evt->get_name(), evt->id, evt->priority);
                auto ret = broadcastEvent(uniqueEvt);
                handlerAmount = ret.first;
                evt = ret.second;
            }
                break;
        }
    }

    for (EventInterceptInfo const &info : allEventInterceptorsCopy) {
        ServiceExecutionScope serviceScope{info.listeningServiceId};
        info.postIntercept(*evt, allowProcessing && handlerAmount > 0);
    }

    for (EventInterceptInfo const &info : eventInterceptorsCopy) {
        ServiceExecutionScope serviceScope{info.listeningServiceId};
        info.postIntercept(*evt, allowProcessing && handlerAmount > 0);
    }

    if(uniqueEvt) {
        handleEventCompletion(*uniqueEvt);
    }
}

void Ichor::DependencyManager::stop() {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (Ichor::Detail::_local_dm != nullptr && this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    for(auto &[key, manager] : _services) {
        if(manager->getServiceState() != ServiceState::ACTIVE) {
            continue;
        }

        ServiceExecutionScope serviceScope{manager->serviceId()};
        auto stopGen = manager->stop();
        auto _ = stopGen.begin();
    }

    _services.clear();

    if(_communicationChannel != nullptr) {
        _communicationChannel->removeManager(this);
    }

    _started.store(false, std::memory_order_release);
    Ichor::Detail::_local_dm = nullptr;
}

void Ichor::DependencyManager::addInternalServiceManager(std::unique_ptr<ILifecycleManager> svc) {
    _services.emplace(svc->serviceId(), std::move(svc));
}

void Ichor::DependencyManager::clearServiceRegistrations(std::vector<EventInterceptInfo> &allEventInterceptorsCopy, std::vector<EventInterceptInfo> &eventInterceptorsCopy, ServiceIdType svcId) {

    for(auto trackers = _dependencyRequestTrackers.begin(); trackers != _dependencyRequestTrackers.end(); ) {
        std::erase_if(trackers->second, [&svcId](DependencyTrackerInfo const &info) {
            return info.svcId == svcId;
        });
        if(trackers->second.empty()) {
            trackers = _dependencyRequestTrackers.erase(trackers);
        } else {
            ++trackers;
        }
    }

    for(auto callbacks = _eventCallbacks.begin(); callbacks != _eventCallbacks.end(); ) {
        std::erase_if(callbacks->second, [&svcId](EventCallbackInfo const &info) {
            return info.listeningServiceId == svcId;
        });
        if(callbacks->second.empty()) {
            callbacks = _eventCallbacks.erase(callbacks);
        } else {
            ++callbacks;
        }
    }

    for(auto interceptors = _eventInterceptors.begin(); interceptors != _eventInterceptors.end(); ) {
        std::erase_if(interceptors->second, [&svcId](EventInterceptInfo const &info) {
            return info.listeningServiceId == svcId;
        });
        if(interceptors->second.empty()) {
            interceptors = _eventInterceptors.erase(interceptors);
        } else {
            ++interceptors;
        }
    }

    std::erase_if(allEventInterceptorsCopy, [&svcId](EventInterceptInfo const &info) {
        return info.listeningServiceId == svcId;
    });

    std::erase_if(eventInterceptorsCopy, [&svcId](EventInterceptInfo const &info) {
        return info.listeningServiceId == svcId;
    });
    INTERNAL_DEBUG("cleared registrations for {}", svcId);
}

void Ichor::DependencyManager::removeInternalService(std::vector<EventInterceptInfo> &allEventInterceptorsCopy, std::vector<EventInterceptInfo> &eventInterceptorsCopy, ServiceIdType svcId) {
    auto const svcIt = _services.find(svcId);
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (svcIt == _services.end()) [[unlikely]] {
            ICHOR_EMERGENCY_LOG2(_logger, "Couldn't remove service with id {}. Please file a bug.", svcId);
            std::terminate();
        }

        auto &dependencies = svcIt->second->getDependencies();
        auto &dependees = svcIt->second->getDependees();

        if(!dependees.empty()) {
            ICHOR_EMERGENCY_LOG2(_logger, "Service {}:{} has dependees remaining. Please file a bug.", svcId, svcIt->second->implementationName());
            std::terminate();
        }
        if(!dependencies.empty()) {
            ICHOR_EMERGENCY_LOG2(_logger, "Service {}:{} has dependencies remaining. Please file a bug.", svcId, svcIt->second->implementationName());
            std::terminate();
        }


    auto existingCoroutineEvent = std::find_if(_scopedEvents.begin(), _scopedEvents.end(), [svcId](const std::pair<uint64_t, v1::ReferenceCountedPointer<Event>> &t) {
        auto evtType = t.second->get_type();
        switch(evtType) {
            case StartServiceEvent::TYPE:
                return t.second->originatingService == svcId || static_cast<StartServiceEvent*>(t.second.get())->serviceId == svcId;
            case ContinuableDependencyOfflineEvent::TYPE:
                return static_cast<ContinuableDependencyOfflineEvent*>(t.second.get())->originatingOfflineServiceId == svcId;
            case DependencyOfflineEvent::TYPE:
            case StopServiceEvent::TYPE:
                return false;
            default:
                return t.second->originatingService == svcId;
        }
    });

        if(existingCoroutineEvent != _scopedEvents.end()) {
            ICHOR_EMERGENCY_LOG2(_logger, "service {}:{} evt {}", existingCoroutineEvent->second->originatingService, svcIt->second->implementationName(), existingCoroutineEvent->second->get_name());
            std::terminate();
        }
    }

    auto depReg = svcIt->second->getDependencyRegistry();
    if(depReg != nullptr) {
        for(auto &dep : *depReg) {
            // this looks a lot like processEvent(), but because calling processEvent() directly would cause a lot of overhead, duplicate part of it.
            v1::ReferenceCountedPointer<DependencyUndoRequestEvent> depUndoReqEvt = v1::make_reference_counted<DependencyUndoRequestEvent>(_eventQueue->getNextEventId(), svcId, INTERNAL_DEPENDENCY_EVENT_PRIORITY, std::get<Dependency>(dep.second), std::get<tl::optional<Properties>>(dep.second));

            uint64_t handlerAmount = 0;

            for (EventInterceptInfo const &info : allEventInterceptorsCopy) {
                info.preIntercept(*depUndoReqEvt);
            }

            for (EventInterceptInfo const &info : eventInterceptorsCopy) {
                info.preIntercept(*depUndoReqEvt);
            }

            auto const trackers = _dependencyRequestTrackers.find(depUndoReqEvt->dependency.interfaceNameHash);
            if (trackers != end(_dependencyRequestTrackers)) {
                for (DependencyTrackerInfo const &info : trackers->second) {
                    auto trackingSvc = _services.find(info.svcId);

                    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                        if(trackingSvc == _services.end()) [[unlikely]] {
                            ICHOR_EMERGENCY_LOG2(_logger, "Failed to find service {} tracking DependencyUndoRequestEvent", info.svcId);
                            std::terminate();
                        }
                    }

                    fmt::println("DependencyUndoRequestEvent for {} tracker {}:{}", svcId, info.svcId, trackingSvc->second->implementationName());
                    INTERNAL_DEBUG("DependencyUndoRequestEvent tracker {}", info.svcId);
                    ServiceExecutionScope serviceScope{info.svcId};
                    auto gen = info.untrackFunc(*depUndoReqEvt);
                    gen.set_priority(depUndoReqEvt->priority);
                    auto it = gen.begin();

                    if(!it.get_finished()) {
                        fmt::println("DependencyUndoRequestEvent !finished for {} tracker {}:{}", svcId, info.svcId, trackingSvc->second->implementationName());
                        gen.set_service_id_stack(std::move(serviceScope.getServiceIdStack()));
                        _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<IchorBehaviour>>(std::move(gen)));
                        _scopedEvents.emplace(it.get_promise_id(), v1::make_reference_counted<DependencyUndoRequestEvent>(_eventQueue->getNextEventId(), info.svcId, INTERNAL_DEPENDENCY_EVENT_PRIORITY, std::get<Dependency>(dep.second), std::get<tl::optional<Properties>>(dep.second), depUndoReqEvt));
                        // _scopedEvents.emplace(it.get_promise_id(), depUndoReqEvt);
                    }

                    handlerAmount++;
                }
            }

            for (EventInterceptInfo const &info : allEventInterceptorsCopy) {
                info.postIntercept(*depUndoReqEvt, handlerAmount > 0);
            }

            for (EventInterceptInfo const &info : eventInterceptorsCopy) {
                info.postIntercept(*depUndoReqEvt, handlerAmount > 0);
            }
        }
    }


    for(auto trackers = _dependencyRequestTrackers.begin(); trackers != _dependencyRequestTrackers.end(); ) {
        std::erase_if(trackers->second, [&svcId](DependencyTrackerInfo const &info) {
            return info.svcId == svcId;
        });
        if(trackers->second.empty()) {
            trackers = _dependencyRequestTrackers.erase(trackers);
        } else {
            trackers++;
        }
    }

    fmt::println("Removed {}:{}", svcId, svcIt->second->implementationName());
    INTERNAL_DEBUG("Removed {}:{}", svcId, svcIt->second->implementationName());
    _services.erase(svcIt);
}

bool Ichor::DependencyManager::existingCoroutineFor(ServiceIdType serviceId) const noexcept {
    auto existingGenerator = std::find_if(_scopedGenerators.begin(), _scopedGenerators.end(), [serviceId](const std::pair<uint64_t, std::unique_ptr<IGenerator>> &t) {
        auto const &stack = t.second->get_service_id_stack();
        return std::find(stack.begin(), stack.end(), serviceId) != stack.end();
    });

    if constexpr (DO_INTERNAL_DEBUG) {
        if(existingGenerator != _scopedGenerators.end()) {
            auto evtIt = _scopedEvents.find(existingGenerator->first);

            if(evtIt == _scopedEvents.end()) [[unlikely]] {
                ICHOR_EMERGENCY_LOG2(_logger, "Missing scopedEvent for scopedGenerator with promise {} and serviceId {}", existingGenerator->first, serviceId);
                std::terminate();
            }

            INTERNAL_DEBUG("existingGenerator {} {} {}", serviceId, evtIt->second->get_name(), evtIt->second->originatingService);
        }
    }

    return existingGenerator != _scopedGenerators.end();
}

Ichor::Task<void> Ichor::DependencyManager::waitForService(ServiceIdType serviceId, uint64_t eventType) noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    auto const it = _dependencyWaiters.find(serviceId);
    if(it != _dependencyWaiters.end()) {
        auto &waiter = it->second.events.emplace_back(eventType, std::make_unique<AsyncManualResetEvent>());
        INTERNAL_DEBUG("waitForService {} {} {}", serviceId, eventType, it->second.events.size());
        co_await *waiter.second;
        co_return;
    }
    auto inserted = _dependencyWaiters.emplace(serviceId, EventWaiter{serviceId, eventType});
    INTERNAL_DEBUG("waitForService {} {} 1", serviceId, eventType);
    co_await *inserted.first->second.events.rbegin()->second;
    co_return;
}

bool Ichor::DependencyManager::finishWaitingService(ServiceIdType serviceId, uint64_t eventType, [[maybe_unused]] std::string_view eventName) noexcept {
    bool ret{};
    auto waiter = _dependencyWaiters.find(serviceId);
    std::vector<std::unique_ptr<AsyncManualResetEvent>> evts{};

    // caution: setting events and triggering their coroutines can result in the containers used here, to be modified.
    if(waiter != _dependencyWaiters.end()) {
        do {
            evts.clear();
            for (auto it = waiter->second.events.begin(); it != waiter->second.events.end();) {
                if (it->first == eventType) {
                    INTERNAL_DEBUG("waitForService set {} {} {}", serviceId, eventName, waiter->second.events.size());
                    evts.emplace_back(std::move(it->second));
                    it = waiter->second.events.erase(it);
                    ret = true;
                } else {
                    INTERNAL_DEBUG("waitForService not set {} {} {}", serviceId, it->first, waiter->second.events.size());
                    it++;
                }
            }
            for(auto &evt : evts) {
                evt->set();
            }
            if(!evts.empty()) {
                waiter = _dependencyWaiters.find(serviceId);
            }
        } while(!evts.empty());
        if(waiter->second.events.empty()) {
            INTERNAL_DEBUG("waitForService done {} {} {}", serviceId, eventName, waiter->second.events.size());
            _dependencyWaiters.erase(waiter);
            ret = true;
        }
    }

    return ret;
}

void Ichor::DependencyManager::checkIfCanQuit(std::vector<EventInterceptInfo> &allEventInterceptorsCopy, std::vector<EventInterceptInfo> &eventInterceptorsCopy) noexcept {
    // If a QuitEvent was received, check if this was the last service to stop and quit if so
    if(_quitEventReceived && !_quitDone && _scopedEvents.empty()) {
        bool allServicesStopped{true};
        for (auto const &svcPair : _services) {
            if(svcPair.second->isInternalManager()) {
                continue;
            }

            if (svcPair.second->getServiceState() != ServiceState::INSTALLED) {
                allServicesStopped = false;
                break;
            }
        }
        if(allServicesStopped) {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (!_eventWaiters.empty()) {
                    ICHOR_EMERGENCY_LOG1(_logger, "Bug in Ichor, please submit a bug report.");
                    std::terminate();
                }
                if (!_dependencyWaiters.empty()) {
                    ICHOR_EMERGENCY_LOG1(_logger, "Bug in Ichor, please submit a bug report.");
                    std::terminate();
                }
                if (!_scopedGenerators.empty()) {
                    ICHOR_EMERGENCY_LOG1(_logger, "Bug in Ichor, please submit a bug report.");
                    std::terminate();
                }
            }
            // Only quit once all coroutines and waiters have drained
            if(_eventWaiters.empty() && _dependencyWaiters.empty() && _scopedEvents.empty() && _scopedGenerators.empty()) {
                _eventQueue->quit();
                _services.clear();
                allEventInterceptorsCopy.clear();
                eventInterceptorsCopy.clear();
                _logger = nullptr;
                _quitDone = true;
            }
        }
    }
}

void Ichor::DependencyManager::handleEventCompletion(Ichor::Event const &evt) {
    auto waitingIt = _eventWaiters.find(evt.id);
    if(waitingIt != end(_eventWaiters)) {
        INTERNAL_DEBUG("handleEventCompletion {}:{} {} events.size {}", evt.id, evt.get_name(), evt.originatingService, waitingIt->second.events.size());

        for(auto &asyncEvt : waitingIt->second.events) {
            asyncEvt.second->set();
        }
        // callback above may modify _eventWaiters, invalidating iterators.
        _eventWaiters.erase(evt.id);
    }
}

std::pair<uint64_t, Ichor::Event*> Ichor::DependencyManager::broadcastEvent(std::unique_ptr<Event> &uniqueEvt) {
    auto const registeredListeners = _eventCallbacks.find(uniqueEvt->get_type());

    if(registeredListeners == end(_eventCallbacks)) {
        return {0, uniqueEvt.get()};
    }

    auto const waitingIt = _eventWaiters.find(uniqueEvt->id);

    // Make copy because the vector can be modified in the callback() call.
    std::vector<EventCallbackInfo> callbacksCopy = registeredListeners->second;
    Event *evt = uniqueEvt.get();
    v1::ReferenceCountedPointer<Event> refEvt{};

    for (auto &callbackInfo: callbacksCopy) {
        auto const service = _services.find(callbackInfo.listeningServiceId);
        if (service == end(_services) ||
            (service->second->getServiceState() != ServiceState::ACTIVE && service->second->getServiceState() != ServiceState::INJECTING)) {
            continue;
        }

        if (callbackInfo.filterServiceId.has_value() && *callbackInfo.filterServiceId != evt->originatingService) {
            continue;
        }

        ServiceExecutionScope serviceScope{callbackInfo.listeningServiceId};
        auto gen = callbackInfo.callback(*evt);
        gen.set_priority(service->second->getPriority());

        if (!gen.done()) {
            auto it = gen.begin();

            INTERNAL_DEBUG("state: {} {} {} {} {}", gen.done(), it.get_finished(), it.get_op_state(), it.get_promise_state(), it.get_promise_id());

            if (!it.get_finished()) {
                if constexpr (DO_INTERNAL_DEBUG) {
                    if (!it.get_has_suspended()) [[unlikely]] {
                        std::terminate();
                    }
                }
                INTERNAL_DEBUG("contains3 {} {} {}", it.get_promise_id(), _scopedGenerators.contains(it.get_promise_id()), _scopedGenerators.size() + 1);
                gen.set_service_id_stack(std::move(serviceScope.getServiceIdStack()));
                _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<IchorBehaviour>>(std::move(gen)));
                if(refEvt.has_value()) {
                    _scopedEvents.emplace(it.get_promise_id(), refEvt);
                } else {
                    auto scopedIt = _scopedEvents.emplace(it.get_promise_id(), std::move(uniqueEvt));
                    refEvt = scopedIt.first->second;
                    evt = refEvt.get();
                }
                if (waitingIt != end(_eventWaiters)) {
                    INTERNAL_DEBUG("broadcastEvent {}:{} {} events.size {}", evt->id, evt->get_name(), evt->originatingService, waitingIt->second.events.size());
                }
            }

//            if(!it.get_finished() && it.get_promise_state() != state::value_not_ready_consumer_active) {
//                _eventQueue->pushPrioritisedEvent<ContinuableEvent>(evt->originatingService, evt->priority, it.get_promise_id());
//            }

            if constexpr (DO_INTERNAL_DEBUG) {
                if (it.get_finished() && it.get_has_suspended()) [[unlikely]] {
                    std::terminate();
                }
            }
        }
    }

    return {callbacksCopy.size(), evt};
}

void Ichor::DependencyManager::runForOrQueueEmpty(std::chrono::milliseconds ms) const noexcept {
    auto now = std::chrono::steady_clock::now();
    auto start = now;
    auto end = now + ms;
    while (now < end && !_eventQueue->shouldQuit()) {
        if(now != start && _started.load(std::memory_order_acquire) && _eventQueue->empty() && _scopedGenerators.empty()) {
            INTERNAL_DEBUG("runForOrQueueEmpty {} {} {} {}", now.time_since_epoch().count(), end.time_since_epoch().count(), _eventQueue->empty(), _scopedGenerators.empty());
            return;
        }
        // value of 1ms may lead to races depending on processor speed
        std::this_thread::sleep_for(5ms);
        now = std::chrono::steady_clock::now();
    }
}

/// Get IService by local ID
/// \param id service id
/// \return optional
[[nodiscard]] tl::optional<Ichor::v1::NeverNull<const Ichor::IService*>> Ichor::DependencyManager::getIService(ServiceIdType id) const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    auto const svc = _services.find(id);

    if(svc == _services.end()) {
        return {};
    }

    return svc->second->getIService();
}

tl::optional<Ichor::v1::NeverNull<const Ichor::IService*>> Ichor::DependencyManager::getIService(sole::uuid id) const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    auto svc = std::find_if(_services.begin(), _services.end(), [&id](const auto &svcPair) {
        return svcPair.second->getIService()->getServiceGid() == id;
    });

    if(svc == _services.end()) {
        return {};
    }

    return svc->second->getIService();
}

std::vector<Ichor::Dependency> Ichor::DependencyManager::getDependencyRequestsForService(ServiceIdType svcId) const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    auto const svc = _services.find(svcId);

    if(svc == _services.end()) {
        return {};
    }

    auto reg = svc->second->getDependencyRegistry();

    if(reg == nullptr) {
        return {};
    }

    std::vector<Dependency> ret;

    for(auto const &r : reg->_registrations) {
        ret.emplace_back(std::get<Dependency>(r.second));
    }

    return ret;
}

Ichor::DependencyManager::DependentServicesView Ichor::DependencyManager::getDependentsForService(ServiceIdType svcId) const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    auto const svc = _services.find(svcId);

    if(svc == _services.end()) {
        return Ichor::DependencyManager::DependentServicesView{this, nullptr};
    }

    return Ichor::DependencyManager::DependentServicesView{this, &svc->second->getDependees()};
}

std::span<Ichor::Dependency const> Ichor::DependencyManager::getProvidedInterfacesForService(ServiceIdType svcId) const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    auto const svc = _services.find(svcId);

    if(svc == _services.end()) {
        return {};
    }

    return svc->second->getInterfaces();
}

Ichor::DependencyManager::TrackersView Ichor::DependencyManager::getTrackersForService(ServiceIdType svcId) const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    return TrackersView{&_dependencyRequestTrackers, svcId};
}

std::vector<std::tuple<Ichor::Event const &, std::span<Ichor::ServiceExecutionScopeContents const>>> Ichor::DependencyManager::getServiceIdsWhichHaveActiveCoroutines() const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    std::vector<std::tuple<Event const &, std::span<ServiceExecutionScopeContents const>>> ret;
    ret.reserve(_scopedGenerators.size());

    for(auto const &[promiseId, gen] : _scopedGenerators) {
        auto evtIt = _scopedEvents.find(promiseId);

        if(evtIt == _scopedEvents.end()) [[unlikely]] {
            ICHOR_EMERGENCY_LOG2(_logger, "Missing scopedEvent for scopedGenerator with promise {}", promiseId);
            std::terminate();
        }

        ret.emplace_back(*(evtIt->second.get()), gen->get_service_id_stack());
    }

    return ret;
}

Ichor::DependencyManager::ServicesView Ichor::DependencyManager::getAllServices() const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    return ServicesView{&_services};
}

tl::optional<std::string_view> Ichor::DependencyManager::getImplementationNameFor(ServiceIdType serviceId) const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    auto const service = _services.find(serviceId);

    if(service == end(_services)) {
        return {};
    }

    return service->second->implementationName();
}

ICHOR_PURE_FUNC_ATTR Ichor::IEventQueue& Ichor::DependencyManager::getEventQueue() const noexcept {
    return *_eventQueue;
}

Ichor::Task<tl::expected<void, Ichor::WaitError>> Ichor::DependencyManager::waitForServiceStarted(v1::NeverNull<IService*> svc) {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    if(_quitEventReceived) {
        co_return tl::unexpected(Ichor::WaitError::QUITTING);
    }
    if(svc->getServiceState() == ServiceState::ACTIVE) {
        co_return {};
    }
    co_await waitForService(svc->getServiceId(), StartServiceEvent::TYPE);
    co_return {};
}

Ichor::Task<tl::expected<void, Ichor::WaitError>> Ichor::DependencyManager::waitForServiceStopped(v1::NeverNull<IService*> svc) {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    if(_quitEventReceived) {
        co_return tl::unexpected(Ichor::WaitError::QUITTING);
    }
    if(svc->getServiceState() == ServiceState::ACTIVE) {
        co_return {};
    }
    co_await waitForService(svc->getServiceId(), StopServiceEvent::TYPE);
    co_return {};
}

void Ichor::DependencyManager::setCommunicationChannel(v1::NeverNull<Ichor::CommunicationChannel*> channel) {
    _communicationChannel = channel;
}

void Ichor::DependencyManager::clearCommunicationChannel() {
    _communicationChannel = nullptr;
}

[[nodiscard]] bool Ichor::HasThreadLocalManager() noexcept {
    return Detail::_local_dm != nullptr;
}

[[nodiscard]] Ichor::DependencyManager& Ichor::GetThreadLocalManager() noexcept {
    return *Detail::_local_dm;
}

// Instantiate the async generator
template class Ichor::AsyncGenerator<void>;
template class Ichor::AsyncGenerator<Ichor::IchorBehaviour>;
template class Ichor::AsyncGenerator<Ichor::StartBehaviour>;
template class Ichor::Detail::AsyncGeneratorPromise<Ichor::StartBehaviour>;
template class Ichor::Detail::AsyncGeneratorPromise<Ichor::IchorBehaviour>;

// ugly hack to catch the place where an exception is thrown
// boost asio/beast are absolutely terrible when it comes to debugging these things
// only works on gcc, probably. See https://stackoverflow.com/a/11674810/1460998
#if ICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING
#include <dlfcn.h>
#include <execinfo.h>
#include <typeinfo>
#include <string>
#include <memory>
#include <cxxabi.h>
#include <cstdlib>

extern "C" void __cxa_throw(void *ex, void *info, void (*dest)(void *)) {
    static void (*const rethrow)(void*,void*,void(*)(void*)) __attribute__ ((noreturn)) = (void (*)(void*,void*,void(*)(void*)))dlsym(RTLD_NEXT, "__cxa_throw");
    rethrow(ex,info,dest);
}
#endif
