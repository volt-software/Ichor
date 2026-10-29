#include <algorithm>
#include <thread>
#include <ichor/DependencyManager.h>
#include <ichor/CommunicationChannel.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/dependency_management/InternalService.h>
#include <ichor/dependency_management/InternalServiceLifecycleManager.h>
#include <fmt/format.h>

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
#include <processthreadsapi.h>
#include <fmt/xchar.h>
#endif

constinit std::atomic<uint64_t> Ichor::DependencyManager::_managerIdCounter = 0;
#ifdef ICHOR_ENABLE_INTERNAL_STL_DEBUGGING
std::atomic<uint64_t> Ichor::v1::_rfpCounter = 0;
#endif
// NOLINTNEXTLINE(bugprone-throwing-static-initialization): only allocation failure can throw, which terminates
thread_local Ichor::unordered_set<Ichor::ServiceIdType, Ichor::ServiceIdHash> Ichor::v1::Detail::emptyDependencies{}; // TODO modify ankerl's implementation to have enough constexpr functions to have this be constinit as well.

constinit thread_local Ichor::DependencyManager *Ichor::v1::Detail::_local_dm = nullptr;

#ifdef ICHOR_USE_BACKWARD
#include <backward/backward.h>

namespace backward {
    backward::SignalHandling sh;
}
#endif

Ichor::DependencyManager::DependencyManager(IEventQueue &eventQueue) : _eventQueue(eventQueue) {
    if(Ichor::v1::Detail::_local_dm != nullptr) [[unlikely]] {
        ICHOR_EMERGENCY_LOG1(_logger, "This thread already has a manager");
        std::terminate();
    }

    Ichor::v1::Detail::_local_dm = this;

    auto dmlm = std::make_unique<Ichor::v1::Detail::InternalServiceLifecycleManager<DependencyManager>>(this);
    auto qlm = std::make_unique<Ichor::v1::Detail::InternalServiceLifecycleManager<IEventQueue>>(&_eventQueue);
    _services.emplace(dmlm->serviceId(), std::move(dmlm));
    _services.emplace(qlm->serviceId(), std::move(qlm));
}

void Ichor::DependencyManager::start() {
    ICHOR_LOG_DEBUG(_logger, "starting dm {}", _id);

    _quitEventReceived = false;
    _started.store(true, std::memory_order_release);

    ICHOR_LOG_TRACE(_logger, "depman {} has {} events", _id, _eventQueue.size());

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
                if (!info.preIntercept(*evt)) {
                    allowProcessing = false;
                }
            }
        }

        if (interceptorsForEvent != end(_eventInterceptors)) {
            // Make copy because the vector can be modified in the preIntercept() call.
            eventInterceptorsCopy = interceptorsForEvent->second;
            for (EventInterceptInfo const &info: eventInterceptorsCopy) {
                if (!info.preIntercept(*evt)) {
                    allowProcessing = false;
                }
            }
        }
    }

    if (allowProcessing) {
        // One handler per internal event type, see src/ichor/event_handlers. Anything else is a user event and gets broadcast to its handlers.
        ProcessEventContext ctx{uniqueEvt, evt, allEventInterceptorsCopy, eventInterceptorsCopy};
        switch (evtType) {
            case DependencyOnlineEvent::TYPE: handleEvent(static_cast<DependencyOnlineEvent &>(*evt), ctx); break;
            case DependencyOfflineEvent::TYPE: handleEvent(static_cast<DependencyOfflineEvent &>(*evt), ctx); break;
            case DependencyRequestEvent::TYPE: handleEvent(static_cast<DependencyRequestEvent &>(*evt), ctx); break;
            case QuitEvent::TYPE: handleEvent(static_cast<QuitEvent &>(*evt), ctx); break;
            case InsertServiceEvent::TYPE: handleEvent(static_cast<InsertServiceEvent &>(*evt), ctx); break;
            case StopServiceEvent::TYPE: handleEvent(static_cast<StopServiceEvent &>(*evt), ctx); break;
            case StartServiceEvent::TYPE: handleEvent(static_cast<StartServiceEvent &>(*evt), ctx); break;
            case RemoveServiceEvent::TYPE: handleEvent(static_cast<RemoveServiceEvent &>(*evt), ctx); break;
            case RemoveEventHandlerEvent::TYPE: handleEvent(static_cast<RemoveEventHandlerEvent &>(*evt), ctx); break;
            case RemoveEventInterceptorEvent::TYPE: handleEvent(static_cast<RemoveEventInterceptorEvent &>(*evt), ctx); break;
            case AddTrackerEvent::TYPE: handleEvent(static_cast<AddTrackerEvent &>(*evt), ctx); break;
            case RemoveTrackerEvent::TYPE: handleEvent(static_cast<RemoveTrackerEvent &>(*evt), ctx); break;
            case ContinuableEvent::TYPE: handleEvent(static_cast<ContinuableEvent &>(*evt), ctx); break;
            case ContinuableStartEvent::TYPE: handleEvent(static_cast<ContinuableStartEvent &>(*evt), ctx); break;
            case RunFunctionEvent::TYPE: handleEvent(static_cast<RunFunctionEvent &>(*evt), ctx); break;
            case RunFunctionEventAsync::TYPE: handleEvent(static_cast<RunFunctionEventAsync &>(*evt), ctx); break;
            default: {
                INTERNAL_DEBUG("{} {} {}", evt->get_name(), evt->id, evt->priority);
                auto ret = broadcastEvent(uniqueEvt);
                handlerAmount = ret.first;
                ctx.evt = ret.second;
            }
                break;
        }
        // handlers that took ownership of uniqueEvt repoint ctx.evt to where the event lives now
        evt = ctx.evt;
    }

    for (EventInterceptInfo const &info : allEventInterceptorsCopy) {
        info.postIntercept(*evt, allowProcessing && handlerAmount > 0);
    }

    for (EventInterceptInfo const &info : eventInterceptorsCopy) {
        info.postIntercept(*evt, allowProcessing && handlerAmount > 0);
    }

    if(uniqueEvt) {
        // NOLINTNEXTLINE(bugprone-use-after-move): a moved-from unique_ptr is null, which the if checks
        handleEventCompletion(*uniqueEvt);
    }
}

void Ichor::DependencyManager::stop() {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (Ichor::v1::Detail::_local_dm != nullptr && this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    for(auto &[key, manager] : _services) {
        if(manager->getServiceState() != ServiceState::ACTIVE) {
            continue;
        }

        auto stopGen = manager->stop();
        auto _ = stopGen.begin();
    }

    _services.clear();

    if(_communicationChannel != nullptr) {
        _communicationChannel->removeManager(this);
    }

    _started.store(false, std::memory_order_release);
    Ichor::v1::Detail::_local_dm = nullptr;
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

        auto existingCoroutineEvent = std::ranges::find_if(_scopedGenerators, [svcId](const std::pair<uint64_t, ScopedGenerator> &t) {
            auto evtType = t.second.event->get_type();
            switch(evtType) {
                case StartServiceEvent::TYPE:
                    return t.second.event->originatingService == svcId || static_cast<StartServiceEvent*>(t.second.event.get())->serviceId == svcId;
                case ContinuableDependencyOfflineEvent::TYPE:
                    return static_cast<ContinuableDependencyOfflineEvent*>(t.second.event.get())->originatingOfflineServiceId == svcId;
                case DependencyOfflineEvent::TYPE:
                case StopServiceEvent::TYPE:
                    return false;
                default:
                    return t.second.event->originatingService == svcId;
            }
        });

        if(existingCoroutineEvent != _scopedGenerators.end()) {
            ICHOR_EMERGENCY_LOG2(_logger, "service {}:{} evt {}", existingCoroutineEvent->second.event->originatingService, svcIt->second->implementationName(), existingCoroutineEvent->second.event->get_name());
            std::terminate();
        }
    }

    auto depReg = svcIt->second->getDependencyRegistry();
    if(depReg != nullptr) {
        for(auto &dep : *depReg) {
            // this looks a lot like processEvent(), but because calling processEvent() directly would cause a lot of overhead, duplicate part of it.
            v1::ReferenceCountedPointer<DependencyUndoRequestEvent> depUndoReqEvt = v1::make_reference_counted<DependencyUndoRequestEvent>(_eventQueue.getNextEventId(), svcId, INTERNAL_DEPENDENCY_EVENT_PRIORITY, std::get<Dependency>(dep.second), std::get<tl::optional<Properties>>(dep.second));

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

                    if(trackingSvc->second->getServiceState() >= ServiceState::UNINJECTING || trackingSvc->second->getServiceState() <= ServiceState::INSTALLED) {
                        INTERNAL_DEBUG("DependencyUndoRequestEvent for {} skipping tracker {}:{} status {}", svcId, info.svcId, trackingSvc->second->implementationName(), trackingSvc->second->getServiceState());
                        continue;
                    }

                    // fmt::println("DependencyUndoRequestEvent for {} tracker {}:{}", svcId, info.svcId, trackingSvc->second->implementationName());
                    INTERNAL_DEBUG("DependencyUndoRequestEvent tracker {}", info.svcId);
                    auto gen = info.untrackFunc(*depUndoReqEvt);
                    gen.set_service_id(info.svcId);
                    gen.set_priority(depUndoReqEvt->priority);
                    auto it = gen.begin();

                    if(!it.get_finished()) {
                        INTERNAL_DEBUG("DependencyUndoRequestEvent !finished for {} tracker {}:{}", svcId, info.svcId, trackingSvc->second->implementationName());
                        _scopedGenerators.emplace(it.get_promise_id(), ScopedGenerator{std::make_unique<AsyncGenerator<IchorBehaviour>>(std::move(gen)), v1::make_reference_counted<DependencyUndoRequestEvent>(_eventQueue.getNextEventId(), info.svcId, INTERNAL_DEPENDENCY_EVENT_PRIORITY, std::get<Dependency>(dep.second), std::get<tl::optional<Properties>>(dep.second), depUndoReqEvt)});
                        // _scopedEvents.emplace(it.get_promise_id(), v1::make_reference_counted<DependencyUndoRequestEvent>(_eventQueue.getNextEventId(), info.svcId, INTERNAL_DEPENDENCY_EVENT_PRIORITY, std::get<Dependency>(dep.second), std::get<tl::optional<Properties>>(dep.second), depUndoReqEvt));
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

    // fmt::println("Removed {}:{}", svcId, svcIt->second->implementationName());
    INTERNAL_DEBUG("Removed {}:{}", svcId, svcIt->second->implementationName());
    _services.erase(svcIt);
}

bool Ichor::DependencyManager::existingCoroutineFor(ServiceIdType serviceId) const noexcept {
    auto existingGenerator = std::ranges::find_if(_scopedGenerators, [serviceId](const std::pair<uint64_t, ScopedGenerator> &t) {
        return t.second.generator->get_service_id() == serviceId;
    });

    if constexpr (DO_INTERNAL_DEBUG) {
        if(existingGenerator != _scopedGenerators.end()) {
            INTERNAL_DEBUG("existingGenerator {} {} {}", serviceId, existingGenerator->second.event->get_name(), existingGenerator->second.event->originatingService);
        }
    }

    return existingGenerator != _scopedGenerators.end();
}

Ichor::Task<void> Ichor::DependencyManager::waitForService(ServiceIdType serviceId, uint64_t eventType) noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
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
    if(_quitEventReceived && !_quitDone && _scopedGenerators.empty()) {
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
            if(_eventWaiters.empty() && _dependencyWaiters.empty() && _scopedGenerators.empty()) {
                _eventQueue.quit();
                _services.clear();
                allEventInterceptorsCopy.clear();
                eventInterceptorsCopy.clear();
                _logger = nullptr;
                _quitDone = true;
            }
        }
    }
}

bool Ichor::DependencyManager::hasDependencyWaiter(ServiceIdType serviceId, uint64_t eventType) noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    auto const it = _dependencyWaiters.find(serviceId);
    if(it != _dependencyWaiters.end()) {
        for(auto const &[evtType, evt] : it->second.events) {
            if(evtType == eventType) {
                return true;
            }
        }
    }
    return false;
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

        auto gen = callbackInfo.callback(*evt);
        gen.set_service_id(callbackInfo.listeningServiceId);
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
                
                if(refEvt.has_value()) {
                    _scopedGenerators.emplace(it.get_promise_id(), ScopedGenerator{std::make_unique<AsyncGenerator<IchorBehaviour>>(std::move(gen)), refEvt});
                    // _scopedEvents.emplace(it.get_promise_id(), refEvt);
                } else {
                    auto scopedIt = _scopedGenerators.emplace(it.get_promise_id(), ScopedGenerator{std::make_unique<AsyncGenerator<IchorBehaviour>>(std::move(gen)), std::move(uniqueEvt)});
                    // auto scopedIt = _scopedEvents.emplace(it.get_promise_id(), std::move(uniqueEvt));
                    refEvt = scopedIt.first->second.event;
                    evt = refEvt.get();
                }
                if (waitingIt != end(_eventWaiters)) {
                    INTERNAL_DEBUG("broadcastEvent {}:{} {} events.size {}", evt->id, evt->get_name(), evt->originatingService, waitingIt->second.events.size());
                }
            }

//            if(!it.get_finished() && it.get_promise_state() != state::value_not_ready_consumer_active) {
//                _eventQueue.pushPrioritisedEvent<ContinuableEvent>(evt->originatingService, evt->priority, it.get_promise_id());
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
    while (now < end && !_eventQueue.shouldQuit()) {
        if(now != start && _started.load(std::memory_order_acquire) && _eventQueue.empty() && _scopedGenerators.empty()) {
            INTERNAL_DEBUG("runForOrQueueEmpty {} {} {} {}", now.time_since_epoch().count(), end.time_since_epoch().count(), _eventQueue.empty(), _scopedGenerators.empty());
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
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
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
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    auto svc = std::ranges::find_if(_services, [&id](const auto &svcPair) {
        return svcPair.second->getIService()->getServiceGid() == id;
    });

    if(svc == _services.end()) {
        return {};
    }

    return svc->second->getIService();
}

std::vector<Ichor::Dependency> Ichor::DependencyManager::getDependencyRequestsForService(ServiceIdType svcId) const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
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
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    auto const svc = _services.find(svcId);

    if(svc == _services.end()) {
        return Ichor::DependencyManager::DependentServicesView{._dm=this, ._dependees=nullptr};
    }

    return Ichor::DependencyManager::DependentServicesView{._dm=this, ._dependees=&svc->second->getDependees()};
}

std::span<Ichor::Dependency const> Ichor::DependencyManager::getProvidedInterfacesForService(ServiceIdType svcId) const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
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
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    return TrackersView{._map=&_dependencyRequestTrackers, ._svcId=svcId};
}

#ifdef ICHOR_HAVE_STD_STACKTRACE
std::vector<std::tuple<Ichor::Event const &, Ichor::ServiceIdType, std::stacktrace const &>> Ichor::DependencyManager::getServiceIdsWhichHaveActiveCoroutines() const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    std::vector<std::tuple<Event const &, ServiceIdType, std::stacktrace const &>> ret;
    ret.reserve(_scopedGenerators.size());

    for(auto const &[promiseId, gen] : _scopedGenerators) {
        // auto evtIt = _scopedEvents.find(promiseId);

        // if(evtIt == _scopedEvents.end()) [[unlikely]] {
        //     ICHOR_EMERGENCY_LOG2(_logger, "Missing scopedEvent for scopedGenerator with promise {}", promiseId);
        //     std::terminate();
        // }

        ret.emplace_back(*(gen.event.get()), gen.generator->get_service_id(), gen.generator->get_stacktrace());
    }

    return ret;
}
#endif

Ichor::DependencyManager::ServicesView Ichor::DependencyManager::getAllServices() const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
            ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
            std::terminate();
        }
    }

    return ServicesView{&_services};
}

tl::optional<std::string_view> Ichor::DependencyManager::getImplementationNameFor(ServiceIdType serviceId) const noexcept {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
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
    return _eventQueue;
}

Ichor::Task<tl::expected<void, Ichor::WaitError>> Ichor::DependencyManager::waitForServiceStarted(v1::NeverNull<IService*> svc) {
    if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
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
        if (this != Ichor::v1::Detail::_local_dm) [[unlikely]] {
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
    return Ichor::v1::Detail::_local_dm != nullptr;
}

[[nodiscard]] Ichor::DependencyManager& Ichor::GetThreadLocalManager() noexcept {
    return *Ichor::v1::Detail::_local_dm;
}

// Instantiate the async generator
template class Ichor::AsyncGenerator<void>;
template class Ichor::AsyncGenerator<Ichor::IchorBehaviour>;
template class Ichor::AsyncGenerator<Ichor::StartBehaviour>;
template class Ichor::v1::Detail::AsyncGeneratorPromise<Ichor::StartBehaviour>;
template class Ichor::v1::Detail::AsyncGeneratorPromise<Ichor::IchorBehaviour>;

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
