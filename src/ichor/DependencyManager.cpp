#include <ichor/DependencyManager.h>
#include <ichor/CommunicationChannel.h>
#include <ichor/stl/Any.h>
#include <ichor/events/RunFunctionEvent.h>
#include <csignal>

#ifdef ICHOR_USE_SYSTEM_MIMALLOC
#include <mimalloc-new-delete.h>
#endif

std::atomic<uint64_t> Ichor::DependencyManager::_managerIdCounter = 0;
Ichor::unordered_set<uint64_t> Ichor::Detail::emptyDependencies{};

thread_local Ichor::DependencyManager *Ichor::Detail::_local_dm = nullptr;



void Ichor::DependencyManager::start() {
//    if (_logger == nullptr) {
//        throw std::runtime_error("Trying to start without a framework logger");
//    }

    if (_services.size() < 2) {
        throw std::runtime_error("Trying to start without any registered services");
    }

    ICHOR_LOG_DEBUG(_logger, "starting dm {}", _id);

    ICHOR_LOG_TRACE(_logger, "depman {} has {} events", _id, _eventQueue->size());

    if(Detail::_local_dm != nullptr) {
        throw std::runtime_error("This thread already has a manager");
    }

    Ichor::Detail::_local_dm = this;
    _started = true;

#ifdef __linux__
    pthread_setname_np(pthread_self(), fmt::format("DepMan #{}", _id).c_str());
#endif
}


void Ichor::DependencyManager::processEvent(std::unique_ptr<Event> &&uniqueEvt) {
//      ICHOR_LOG_ERROR(_logger, "evt id {} type {} has {}-{} prio", evt->id, evt->name, evtNode.key(), evt->priority);

    std::shared_ptr<Event> evt{std::move(uniqueEvt)};
    bool allowProcessing = true;
    uint32_t handlerAmount = 1; // for the non-default case below, the DepMan handles the event
    auto interceptorsForAllEvents = _eventInterceptors.find(0);
    auto interceptorsForEvent = _eventInterceptors.find(evt->type);
    std::vector<EventInterceptInfo> allEventInterceptorsCopy{};
    std::vector<EventInterceptInfo> eventInterceptorsCopy{};

    if (interceptorsForAllEvents != end(_eventInterceptors)) {
        // Make copy because the vector can be modified in the preIntercept() call.
        allEventInterceptorsCopy = interceptorsForAllEvents->second;
        for (EventInterceptInfo const &info : allEventInterceptorsCopy) {
            if (!info.preIntercept(*evt)) {
                allowProcessing = false;
            }
        }
    }

    if (interceptorsForEvent != end(_eventInterceptors)) {
        // Make copy because the vector can be modified in the preIntercept() call.
        eventInterceptorsCopy = interceptorsForEvent->second;
        for (EventInterceptInfo const &info : eventInterceptorsCopy) {
            if (!info.preIntercept(*evt)) {
                allowProcessing = false;
            }
        }
    }

    if (allowProcessing) {
        switch (evt->type) {
            case DependencyOnlineEvent::TYPE: {
                INTERNAL_DEBUG("DependencyOnlineEvent {}", evt->id);
                auto *depOnlineEvt = static_cast<DependencyOnlineEvent *>(evt.get());
                auto managerIt = _services.find(depOnlineEvt->originatingService);

                if (managerIt == end(_services)) {
                    break;
                }

                auto &manager = managerIt->second;

                if (!manager->setInjected()) {
                    INTERNAL_DEBUG("Couldn't set injected for {} {} {}", manager->serviceId(), manager->implementationName(), manager->getServiceState());
                    break;
                }

                auto const filterProp = manager->getProperties().find("Filter");
                const Filter *filter = nullptr;
                if (filterProp != cend(manager->getProperties())) {
                    filter = Ichor::any_cast<Filter *const>(&filterProp->second);
                }

                for (auto const &[key, possibleDependentLifecycleManager]: _services) {
                    if (key == depOnlineEvt->originatingService || (filter != nullptr && !filter->compareTo(*possibleDependentLifecycleManager.get()))) {
                        continue;
                    }

                    if (possibleDependentLifecycleManager->dependencyOnline(manager.get()) == Detail::DependencyChange::FOUND_AND_START_ME) {
                        pushPrioritisedEvent<StartServiceEvent>(depOnlineEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY,
                                                             possibleDependentLifecycleManager->serviceId());
                    }
                }
            }
                break;
            case DependencyOfflineEvent::TYPE: {
                INTERNAL_DEBUG("DependencyOfflineEvent {} {}", evt->id, evt->originatingService);
                auto *depOfflineEvt = static_cast<DependencyOfflineEvent *>(evt.get());
                auto managerIt = _services.find(depOfflineEvt->originatingService);

                if (managerIt == end(_services)) {
                    break;
                }

                auto &manager = managerIt->second;

                if (!manager->setUninjected()) {
                    // DependencyOfflineEvent already processed
                    INTERNAL_DEBUG("Couldn't set uninjected for {} {} {}", manager->serviceId(), manager->implementationName(), manager->getServiceState());
                    break;
                }

                // copy dependees as it will be modified during this loop
                auto dependees = manager->getDependees();
                for (auto &serviceId : dependees) {
                    auto depIt = _services.find(serviceId);

                    if(depIt == _services.end()) {
                        continue;
                    }

                    if (depIt->second->dependencyOffline(manager.get()) == Detail::DependencyChange::FOUND_AND_STOP_ME) {
                        INTERNAL_DEBUG("svc {}:{} reported dependency found and to stop ---------", serviceId, depIt->second->implementationName());
                        pushPrioritisedEvent<StopServiceEvent>(depOfflineEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY,
                                                            depIt->second->serviceId());
                    }
                }
            }
                break;
            case DependencyRequestEvent::TYPE: {
                auto *depReqEvt = static_cast<DependencyRequestEvent *>(evt.get());

                auto trackers = _dependencyRequestTrackers.find(depReqEvt->dependency.interfaceNameHash);
                if (trackers == end(_dependencyRequestTrackers)) {
                    break;
                }

                for (DependencyTrackerInfo &info: trackers->second) {
                    info.trackFunc(*depReqEvt);
                }
            }
                break;
            case DependencyUndoRequestEvent::TYPE: {
                auto *depUndoReqEvt = static_cast<DependencyUndoRequestEvent *>(evt.get());

                auto trackers = _dependencyUndoRequestTrackers.find(depUndoReqEvt->dependency.interfaceNameHash);
                if (trackers == end(_dependencyUndoRequestTrackers)) {
                    break;
                }

                for (DependencyTrackerInfo &info: trackers->second) {
                    info.trackFunc(*depUndoReqEvt);
                }
            }
                break;
            case QuitEvent::TYPE: {
                INTERNAL_DEBUG("QuitEvent {}", evt->id);
                auto *_quitEvt = static_cast<QuitEvent *>(evt.get());

                bool allServicesStopped{true};

                for (auto const &[key, possibleManager]: _services) {
                    if (possibleManager->getServiceState() != ServiceState::INSTALLED) {
                        allServicesStopped = false;

                        auto &dependees = possibleManager->getDependees();

                        for(auto &serviceId : dependees) {
                            pushPrioritisedEvent<StopServiceEvent>(_quitEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY,
                                                                serviceId);
                        }

                        pushPrioritisedEvent<StopServiceEvent>(_quitEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY,
                                                            possibleManager->serviceId());
                    }
                }

                if (allServicesStopped) {
                    _eventQueue->quit();
                } else {
                    pushPrioritisedEvent<QuitEvent>(_quitEvt->originatingService, INTERNAL_EVENT_PRIORITY + 1);
                }
            }
                break;
            case StopServiceEvent::TYPE: {
                auto *stopServiceEvt = static_cast<StopServiceEvent *>(evt.get());

                auto toStopServiceIt = _services.find(stopServiceEvt->serviceId);

                if (toStopServiceIt == end(_services)) {
                    ICHOR_LOG_ERROR(_logger, "Couldn't stop service {}, missing from known services", stopServiceEvt->serviceId);
                    handleEventError(*stopServiceEvt);
                    break;
                }

                auto *toStopService = toStopServiceIt->second.get();

                INTERNAL_DEBUG("StopServiceEvent {} {}:{} state {} dependees {}", evt->id, stopServiceEvt->serviceId, toStopService->implementationName(), toStopService->getServiceState(), toStopService->getDependees().size());

                // already stopped
                if(toStopService->getServiceState() == ServiceState::INSTALLED) {
                    INTERNAL_DEBUG("already stopped");
                    break;
                }

                auto existingCoroutineEvent = std::find_if(_scopedEvents.begin(), _scopedEvents.end(), [&toStopService](const std::pair<uint64_t, std::shared_ptr<Event>> &t) {
                    return t.second->originatingService == toStopService->serviceId();
                });

                // coroutine needs to finish before we can stop the service
                if(existingCoroutineEvent != _scopedEvents.end()) {
                    INTERNAL_DEBUG("existing scoped event");
                    pushPrioritisedEvent<StopServiceEvent>(stopServiceEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY,
                                                        toStopService->serviceId());
                    break;
                }

                auto &dependencies = toStopService->getDependencies();
                auto &dependees = toStopService->getDependees();

                for (auto &serviceId : dependencies) {
                    INTERNAL_DEBUG("dependency: {}", serviceId);
                }

                for (auto &serviceId : dependees) {
                    INTERNAL_DEBUG("dependee: {}", serviceId);
                }

                // If all services depending on this one have removed this one and we've had a DependencyOfflineEvent
                if (dependees.empty() && (toStopService->getServiceState() == ServiceState::UNINJECTING || toStopService->getServiceState() == ServiceState::STOPPING)) {
                    auto ret = toStopService->stop();
                    INTERNAL_DEBUG("service->stop() ret {} state {}", ret, toStopService->getServiceState());
                    if (toStopService->getServiceState() != ServiceState::INSTALLED && ret != StartBehaviour::SUCCEEDED) {
                        handleEventError(*stopServiceEvt);
                        if (ret == StartBehaviour::FAILED_AND_RETRY) {
                            pushPrioritisedEvent<StopServiceEvent>(stopServiceEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY,
                                                                stopServiceEvt->serviceId);
                        }
                    } else {
                        for (auto &serviceId : dependencies) {
                            auto depIt = _services.find(serviceId);

                            if(depIt == _services.end()) {
                                continue;
                            }

                            depIt->second->getDependees().erase(stopServiceEvt->serviceId);
                        }
                        handleEventCompletion(*stopServiceEvt);
                    }
                } else {
                    pushPrioritisedEvent<DependencyOfflineEvent>(toStopService->serviceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY - 1);
                    pushPrioritisedEvent<StopServiceEvent>(stopServiceEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY, stopServiceEvt->serviceId);
                }
            }
                break;
            case RemoveServiceEvent::TYPE: {
                INTERNAL_DEBUG("RemoveServiceEvent {}", evt->id);
                auto *removeServiceEvt = static_cast<RemoveServiceEvent *>(evt.get());

                auto toRemoveServiceIt = _services.find(removeServiceEvt->serviceId);

                if (toRemoveServiceIt == end(_services)) {
                    ICHOR_LOG_ERROR(_logger, "Couldn't remove service {}, missing from known services", removeServiceEvt->serviceId);
                    handleEventError(*removeServiceEvt);
                    break;
                }

                auto *toRemoveService = toRemoveServiceIt->second.get();
                if (removeServiceEvt->dependenciesStopped) {
                    auto ret = toRemoveService->stop();
                    if (toRemoveService->getServiceState() == ServiceState::ACTIVE && ret != StartBehaviour::SUCCEEDED) {
                        ICHOR_LOG_ERROR(_logger, "Couldn't remove service {}: {} but all dependencies stopped", removeServiceEvt->serviceId,
                                        toRemoveService->implementationName());
                        handleEventError(*removeServiceEvt);
                        if (ret == StartBehaviour::FAILED_AND_RETRY) {
                            pushPrioritisedEvent<RemoveServiceEvent>(removeServiceEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY,
                                                                  removeServiceEvt->serviceId,
                                                                  true);
                        }
                    } else {
                        handleEventCompletion(*removeServiceEvt);
                        _services.erase(toRemoveServiceIt);
                    }
                } else {
                    pushPrioritisedEvent<DependencyOfflineEvent>(toRemoveService->serviceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY);
                    pushPrioritisedEvent<RemoveServiceEvent>(removeServiceEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY,
                                                          removeServiceEvt->serviceId,
                                                          true);
                }
            }
                break;
            case StartServiceEvent::TYPE: {
                INTERNAL_DEBUG("StartServiceEvent {}", evt->id);
                auto *startServiceEvt = static_cast<StartServiceEvent *>(evt.get());

                auto toStartServiceIt = _services.find(startServiceEvt->serviceId);

                if (toStartServiceIt == end(_services)) {
                    ICHOR_LOG_ERROR(_logger, "Couldn't start service {}, missing from known services", startServiceEvt->serviceId);
                    handleEventError(*startServiceEvt);
                    break;
                }

                auto *toStartService = toStartServiceIt->second.get();
                if (toStartService->getServiceState() == ServiceState::ACTIVE) {
                    handleEventCompletion(*startServiceEvt);
                } else {
                    auto ret = toStartService->start();
                    if (ret == StartBehaviour::SUCCEEDED) {
                        pushPrioritisedEvent<DependencyOnlineEvent>(toStartService->serviceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY);
                        handleEventCompletion(*startServiceEvt);
                    } else {
                        INTERNAL_DEBUG("Couldn't start service {}: {}", startServiceEvt->serviceId, toStartService->implementationName());
                        handleEventError(*startServiceEvt);
                        if (ret == StartBehaviour::FAILED_AND_RETRY) {
                            pushPrioritisedEvent<StartServiceEvent>(startServiceEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY,
                                                                 startServiceEvt->serviceId);
                        }
                    }
                }
            }
                break;
            case DoWorkEvent::TYPE: {
                INTERNAL_DEBUG("DoWorkEvent {}", evt->id);
                handleEventCompletion(*evt);
            }
                break;
            case RemoveCompletionCallbacksEvent::TYPE: {
                INTERNAL_DEBUG("RemoveCompletionCallbacksEvent {}", evt->id);
                auto *removeCallbacksEvt = static_cast<RemoveCompletionCallbacksEvent *>(evt.get());

                _completionCallbacks.erase(removeCallbacksEvt->key);
                _errorCallbacks.erase(removeCallbacksEvt->key);
            }
                break;
            case RemoveEventHandlerEvent::TYPE: {
                INTERNAL_DEBUG("RemoveEventHandlerEvent {}", evt->id);
                auto *removeEventHandlerEvt = static_cast<RemoveEventHandlerEvent *>(evt.get());

                // key.id = service id, key.type == event id
                auto existingHandlers = _eventCallbacks.find(removeEventHandlerEvt->key.type);
                if (existingHandlers != end(_eventCallbacks)) {
                    std::erase_if(existingHandlers->second, [removeEventHandlerEvt](const EventCallbackInfo &info) noexcept {
                        return info.listeningServiceId == removeEventHandlerEvt->key.id;
                    });
                }
            }
                break;
            case RemoveEventInterceptorEvent::TYPE: {
                INTERNAL_DEBUG("RemoveEventInterceptorEvent {}", evt->id);
                auto *removeEventHandlerEvt = static_cast<RemoveEventInterceptorEvent *>(evt.get());

                // key.id = service id, key.type == event id
                auto existingHandlers = _eventInterceptors.find(removeEventHandlerEvt->key.type);
                if (existingHandlers != end(_eventInterceptors)) {
                    std::erase_if(existingHandlers->second, [removeEventHandlerEvt](const EventInterceptInfo &info) noexcept {
                        return info.listeningServiceId == removeEventHandlerEvt->key.id;
                    });
                }
            }
                break;
            case RemoveTrackerEvent::TYPE: {
                INTERNAL_DEBUG("RemoveTrackerEvent {}", evt->id);
                auto *removeTrackerEvt = static_cast<RemoveTrackerEvent *>(evt.get());

                _dependencyRequestTrackers.erase(removeTrackerEvt->interfaceNameHash);
                _dependencyUndoRequestTrackers.erase(removeTrackerEvt->interfaceNameHash);
            }
                break;
            case ContinuableEvent::TYPE: {
                INTERNAL_DEBUG("ContinuableEventAsync {}", evt->id);
                auto *continuableEvt = static_cast<ContinuableEvent *>(evt.get());
                auto genIt = _scopedGenerators.find(continuableEvt->promiseId);

                if (genIt != _scopedGenerators.end()) {
                    INTERNAL_DEBUG("ContinuableEventAsync2 {}", genIt->second->done());

                    if (!genIt->second->done()) {
                        auto it = genIt->second->begin_interface();
                        INTERNAL_DEBUG("ContinuableEventAsync it {} {} {}", it->get_finished(), it->get_op_state(), it->get_promise_state());

                        if (!it->get_finished() && it->get_promise_state() != state::value_not_ready_consumer_active) {
                            pushPrioritisedEvent<ContinuableEvent>(continuableEvt->originatingService, evt->priority, continuableEvt->promiseId);
                        }

                        if (it->get_finished()) {
                            INTERNAL_DEBUG("removed1 {} {}", continuableEvt->promiseId, _scopedGenerators.size() - 1);
                            _scopedGenerators.erase(continuableEvt->promiseId);
                            _scopedEvents.erase(continuableEvt->promiseId);
                        }
                    } else {
                        INTERNAL_DEBUG("removed2 {} {}", continuableEvt->promiseId, _scopedGenerators.size() - 1);
                        _scopedGenerators.erase(continuableEvt->promiseId);
                        _scopedEvents.erase(continuableEvt->promiseId);
                    }
                }
            }
                break;
            case RunFunctionEvent::TYPE: {
                INTERNAL_DEBUG("RunFunctionEvent {}", evt->id);
                auto *runFunctionEvt = static_cast<RunFunctionEvent *>(evt.get());

                // Do not handle stale run function events
                if(runFunctionEvt->originatingService != 0) {
                    auto requestingServiceIt = _services.find(runFunctionEvt->originatingService);
                    if(requestingServiceIt != end(_services) && requestingServiceIt->second->getServiceState() == ServiceState::INSTALLED) {
                        INTERNAL_DEBUG("Service {}:{} not active", runFunctionEvt->originatingService, requestingServiceIt->second->implementationName());
                        break;
                    }
                }

                auto gen = runFunctionEvt->fun(*this);
                auto it = gen.begin();
                INTERNAL_DEBUG("state: {} {} {} {} {}", gen.done(), it.get_finished(), it.get_op_state(), it.get_promise_state(), it.get_promise_id());

                if (!it.get_finished() && it.get_promise_state() != state::value_not_ready_consumer_active) {
                    pushPrioritisedEvent<ContinuableEvent>(runFunctionEvt->originatingService, evt->priority, it.get_promise_id());
                }

                if (!it.get_finished()) {
                    INTERNAL_DEBUG("contains2 {} {} {}", it.get_promise_id(), _scopedGenerators.contains(it.get_promise_id()),
                                   _scopedGenerators.size() + 1);
                    _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<void>>(std::move(gen)));
                    _scopedEvents.emplace(it.get_promise_id(), evt);
                } else {
                    INTERNAL_DEBUG("removed3 {} {}", it.get_promise_id(), _scopedGenerators.size() - 1);
                    _scopedGenerators.erase(it.get_promise_id());
                    _scopedEvents.erase(it.get_promise_id());
                }
            }
                break;
            default: {
                INTERNAL_DEBUG("broadcastEvent {}", evt->id);
                handlerAmount = broadcastEvent(evt);
            }
                break;
        }
    }

    if (interceptorsForAllEvents != end(_eventInterceptors)) {
        for (EventInterceptInfo const &info : allEventInterceptorsCopy) {
            info.postIntercept(*evt, allowProcessing && handlerAmount > 0);
        }
    }

    if (interceptorsForEvent != end(_eventInterceptors)) {
        for (EventInterceptInfo const &info : eventInterceptorsCopy) {
            info.postIntercept(*evt, allowProcessing && handlerAmount > 0);
        }
    }

}

void Ichor::DependencyManager::stop() {
    for(auto &[key, manager] : _services) {
        auto _ = manager->stop();
    }

    _services.clear();

    if(_communicationChannel != nullptr) {
        _communicationChannel->removeManager(this);
    }

    _started = false;
    Ichor::Detail::_local_dm = nullptr;
}

void Ichor::DependencyManager::handleEventCompletion(Ichor::Event const &evt) {
    if(evt.originatingService == 0) {
        return;
    }

    auto service = _services.find(evt.originatingService);
    if(service == end(_services) || (service->second->getServiceState() != ServiceState::ACTIVE && service->second->getServiceState() != ServiceState::INJECTING)) {
        return;
    }

    auto callback = _completionCallbacks.find(CallbackKey{evt.originatingService, evt.type});
    if(callback == end(_completionCallbacks)) {
        return;
    }

    callback->second(evt);
}

uint32_t Ichor::DependencyManager::broadcastEvent(std::shared_ptr<Event> &evt) {
    auto registeredListeners = _eventCallbacks.find(evt->type);

    if(registeredListeners == end(_eventCallbacks)) {
        return 0;
    }

    // Make copy because the vector can be modified in the callback() call.
    std::vector<EventCallbackInfo> callbacksCopy = registeredListeners->second;

    for(auto &callbackInfo : callbacksCopy) {
        auto service = _services.find(callbackInfo.listeningServiceId);
        if(service == end(_services) || (service->second->getServiceState() != ServiceState::ACTIVE && service->second->getServiceState() != ServiceState::INJECTING)) {
            continue;
        }

        if(callbackInfo.filterServiceId.has_value() && *callbackInfo.filterServiceId != evt->originatingService) {
            continue;
        }

        auto gen = callbackInfo.callback(*evt);

        if(!gen.done()) {
            auto it = gen.begin();

            INTERNAL_DEBUG("state: {} {} {} {} {}", gen.done(), it.get_finished(), it.get_op_state(), it.get_promise_state(), it.get_promise_id());

            if(!it.get_finished()) {
                INTERNAL_DEBUG("contains3 {} {} {}", it.get_promise_id(), _scopedGenerators.contains(it.get_promise_id()), _scopedGenerators.size() + 1);
                _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<void>>(std::move(gen)));
            }

            if(!it.get_finished() && it.get_promise_state() != state::value_not_ready_consumer_active) {
                pushPrioritisedEvent<ContinuableEvent>(evt->originatingService, evt->priority, it.get_promise_id());
                _scopedEvents.emplace(it.get_promise_id(), evt);
            }

            if(it.get_finished()) {
                INTERNAL_DEBUG("removed4 {} {}", it.get_promise_id(), _scopedGenerators.size() - 1);
                _scopedGenerators.erase(it.get_promise_id());
            }
        }
    }

    return callbacksCopy.size();
}

void Ichor::DependencyManager::runForOrQueueEmpty(std::chrono::milliseconds ms) const noexcept {
    auto now = std::chrono::steady_clock::now();
    auto start = now;
    auto end = now + ms;
    while (now < end && !_eventQueue->shouldQuit()) {
        if(now != start && _started && _eventQueue->empty()) {
            return;
        }
        // value of 1ms may lead to races depending on processor speed
        std::this_thread::sleep_for(5ms);
        now = std::chrono::steady_clock::now();
    }
}

std::optional<std::string_view> Ichor::DependencyManager::getImplementationNameFor(uint64_t serviceId) const noexcept {
    auto service = _services.find(serviceId);

    if(service == end(_services)) {
        return {};
    }

    return service->second->implementationName();
}

uint64_t Ichor::DependencyManager::getNextEventId() noexcept {
    return _eventIdCounter.fetch_add(1, std::memory_order_acq_rel);
}

void Ichor::DependencyManager::setCommunicationChannel(Ichor::CommunicationChannel *channel) {
    _communicationChannel = channel;
}

Ichor::EventCompletionHandlerRegistration::~EventCompletionHandlerRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushPrioritisedEvent<RemoveCompletionCallbacksEvent>(0, _priority, _key);
    }
}

void Ichor::EventCompletionHandlerRegistration::reset() {
    if(_mgr != nullptr) {
        _mgr->pushPrioritisedEvent<RemoveCompletionCallbacksEvent>(0, _priority, _key);
        _mgr = nullptr;
    }
}

Ichor::EventHandlerRegistration::~EventHandlerRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushPrioritisedEvent<RemoveEventHandlerEvent>(0, _priority, _key);
    }
}

void Ichor::EventHandlerRegistration::reset() {
    if(_mgr != nullptr) {
        _mgr->pushPrioritisedEvent<RemoveEventHandlerEvent>(0, _priority, _key);
        _mgr = nullptr;
    }
}

Ichor::EventInterceptorRegistration::~EventInterceptorRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushPrioritisedEvent<RemoveEventInterceptorEvent>(0, _priority, _key);
    }
}

void Ichor::EventInterceptorRegistration::reset() {
    if(_mgr != nullptr) {
        _mgr->pushPrioritisedEvent<RemoveEventInterceptorEvent>(0, _priority, _key);
        _mgr = nullptr;
    }
}

Ichor::DependencyTrackerRegistration::~DependencyTrackerRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushPrioritisedEvent<RemoveTrackerEvent>(0, _priority, _interfaceNameHash);
    }
}

void Ichor::DependencyTrackerRegistration::reset() {
    if(_mgr != nullptr) {
        _mgr->pushPrioritisedEvent<RemoveTrackerEvent>(0, _priority, _interfaceNameHash);
        _mgr = nullptr;
    }
}


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