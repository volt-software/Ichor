#include <ichor/DependencyManager.h>
#include <ichor/CommunicationChannel.h>
#include <ichor/stl/Any.h>
#include <ichor/GetThreadLocalMemoryResource.h>

std::atomic<bool> sigintQuit;
std::atomic<uint64_t> Ichor::DependencyManager::_managerIdCounter = 0;

void on_sigint([[maybe_unused]] int sig) {
    sigintQuit.store(true, std::memory_order_release);
}



void Ichor::DependencyManager::start() {
    setThreadLocalMemoryResource(_memResource);

    if(_logger == nullptr) {
        throw std::runtime_error("Trying to start without a framework logger");
    }

    if(_services.size() < 2) {
        throw std::runtime_error("Trying to start without any registered services");
    }

    ICHOR_LOG_DEBUG(_logger, "starting dm {}", _id);

    ::signal(SIGINT, on_sigint);

    ICHOR_LOG_TRACE(_logger, "depman {} has {} events", _id, _eventQueue.size());

    _started = true;

#ifdef __linux__
    pthread_setname_np(pthread_self(), fmt::format("DepMan #{}", _id).c_str());
#endif

    while(!_quit.load(std::memory_order_acquire)) {
        _quit.store(sigintQuit.load(std::memory_order_acquire), std::memory_order_release);
        std::shared_lock lck(_eventQueueMutex);
        while (!_quit.load(std::memory_order_acquire) && !_eventQueue.empty()) {
            TSAN_ANNOTATE_HAPPENS_AFTER((void*)&(*_eventQueue.begin()));
            auto evtNode = _eventQueue.extract(_eventQueue.begin());
            lck.unlock();
            _quit.store(sigintQuit.load(std::memory_order_acquire), std::memory_order_release);

//            ICHOR_LOG_ERROR(_logger, "evt id {} type {} has {}-{} prio", evtNode.mapped().get()->id, evtNode.mapped().get()->name, evtNode.key(), evtNode.mapped().get()->priority);

            bool allowProcessing = true;
            uint32_t handlerAmount = 1; // for the non-default case below, the DepMan handles the event
            auto interceptorsForAllEvents = _eventInterceptors.find(0);
            auto interceptorsForEvent = _eventInterceptors.find(evtNode.mapped()->type);

            if(interceptorsForAllEvents != end(_eventInterceptors)) {
                for(EventInterceptInfo &info : interceptorsForAllEvents->second) {
                    if(!info.preIntercept(evtNode.mapped().get())) {
                        allowProcessing = false;
                    }
                }
            }

            if(interceptorsForEvent != end(_eventInterceptors)) {
                for(EventInterceptInfo &info : interceptorsForEvent->second) {
                    if(!info.preIntercept(evtNode.mapped().get())) {
                        allowProcessing = false;
                    }
                }
            }

            if(allowProcessing) {
                switch (evtNode.mapped()->type) {
                    case DependencyOnlineEvent::TYPE: {
                        INTERNAL_DEBUG("DependencyOnlineEvent");
                        auto depOnlineEvt = static_cast<DependencyOnlineEvent *>(evtNode.mapped().get());
                        auto managerIt = _services.find(depOnlineEvt->originatingService);

                        if(managerIt == end(_services)) {
                            break;
                        }

                        auto &manager = managerIt->second;

                        if(!manager->setInjected()) {
                            INTERNAL_DEBUG("Couldn't set injected for {} {} {}", manager->serviceId(), manager->implementationName(), manager->getServiceState());
                            break;
                        }

                        auto const filterProp = manager->getProperties().find("Filter");
                        const Filter *filter = nullptr;
                        if (filterProp != cend(manager->getProperties())) {
                            filter = Ichor::any_cast<Filter * const>(&filterProp->second);
                        }

                        for (auto const &[key, possibleDependentLifecycleManager] : _services) {
                            if (filter != nullptr && !filter->compareTo(possibleDependentLifecycleManager)) {
                                continue;
                            }

                            if(possibleDependentLifecycleManager->dependencyOnline(manager.get())) {
                                pushEventInternal<StartServiceEvent>(depOnlineEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY, possibleDependentLifecycleManager->serviceId());
                            }
                        }
                    }
                        break;
                    case DependencyOfflineEvent::TYPE: {
                        INTERNAL_DEBUG("DependencyOfflineEvent");
                        auto depOfflineEvt = static_cast<DependencyOfflineEvent *>(evtNode.mapped().get());
                        auto managerIt = _services.find(depOfflineEvt->originatingService);

                        if(managerIt == end(_services)) {
                            break;
                        }

                        auto &manager = managerIt->second;

                        if(!manager->setUninjected()) {
                            INTERNAL_DEBUG("Couldn't set uninjected for {} {} {}", manager->serviceId(), manager->implementationName(), manager->getServiceState());
                            break;
                        }

                        auto const filterProp = manager->getProperties().find("Filter");
                        const Filter *filter = nullptr;
                        if (filterProp != cend(manager->getProperties())) {
                            filter = Ichor::any_cast<Filter * const>(&filterProp->second);
                        }

                        for (auto const &[key, possibleDependentLifecycleManager] : _services) {
                            if (filter != nullptr && !filter->compareTo(possibleDependentLifecycleManager)) {
                                continue;
                            }

                            if(possibleDependentLifecycleManager->dependencyOffline(manager.get())) {
                                pushEventInternal<StopServiceEvent>(depOfflineEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY, possibleDependentLifecycleManager->serviceId());
                            }
                        }
                    }
                        break;
                    case DependencyRequestEvent::TYPE: {
                        auto depReqEvt = static_cast<DependencyRequestEvent *>(evtNode.mapped().get());

                        auto trackers = _dependencyRequestTrackers.find(depReqEvt->dependency.interfaceNameHash);
                        if (trackers == end(_dependencyRequestTrackers)) {
                            break;
                        }

                        for (DependencyTrackerInfo &info : trackers->second) {
                            info.trackFunc(depReqEvt);
                        }
                    }
                        break;
                    case DependencyUndoRequestEvent::TYPE: {
                        auto depUndoReqEvt = static_cast<DependencyUndoRequestEvent *>(evtNode.mapped().get());

                        auto trackers = _dependencyUndoRequestTrackers.find(depUndoReqEvt->dependency.interfaceNameHash);
                        if (trackers == end(_dependencyUndoRequestTrackers)) {
                            break;
                        }

                        for (DependencyTrackerInfo &info : trackers->second) {
                            info.trackFunc(depUndoReqEvt);
                        }
                    }
                        break;
                    case QuitEvent::TYPE: {
                        INTERNAL_DEBUG("QuitEvent");
                        auto _quitEvt = static_cast<QuitEvent *>(evtNode.mapped().get());
                        if (!_quitEvt->dependenciesStopped) {
                            for (auto const &[key, possibleManager] : _services) {
                                if (possibleManager->getServiceState() != ServiceState::INSTALLED) {
                                    pushEventInternal<StopServiceEvent>(_quitEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY,
                                                                        possibleManager->serviceId());
                                }
                            }

                            pushEventInternal<QuitEvent>(_quitEvt->originatingService, INTERNAL_EVENT_PRIORITY + 1, true);
                        } else {
                            bool canFinally_quit = true;
                            for (auto const &[key, manager] : _services) {
                                if (manager->getServiceState() != ServiceState::INSTALLED) {
                                    canFinally_quit = false;
                                    INTERNAL_DEBUG("couldn't quit: service {}-{} is in state {}", manager->serviceId(), manager->implementationName(), manager->getServiceState());
                                }
                            }

                            if (canFinally_quit) {
                                _quit.store(true, std::memory_order_release);
                            } else {
                                pushEventInternal<QuitEvent>(_quitEvt->originatingService, INTERNAL_EVENT_PRIORITY + 1, false);
                            }
                        }
                    }
                        break;
                    case StopServiceEvent::TYPE: {
                        INTERNAL_DEBUG("StopServiceEvent");
                        auto stopServiceEvt = static_cast<StopServiceEvent *>(evtNode.mapped().get());

                        auto toStopServiceIt = _services.find(stopServiceEvt->serviceId);

                        if (toStopServiceIt == end(_services)) {
                            ICHOR_LOG_ERROR(_logger, "Couldn't stop service {}, missing from known services", stopServiceEvt->serviceId);
                            handleEventError(stopServiceEvt);
                            break;
                        }

                        auto &toStopService = toStopServiceIt->second;
                        if (stopServiceEvt->dependenciesStopped) {
                            auto ret = toStopService->stop();
                            if (toStopService->getServiceState() != ServiceState::INSTALLED && ret != StartBehaviour::SUCCEEDED) {
                                ICHOR_LOG_ERROR(_logger, "Couldn't stop service {}: {} but all dependencies stopped", stopServiceEvt->serviceId,
                                          toStopService->implementationName());
                                handleEventError(stopServiceEvt);
                                if(ret == StartBehaviour::FAILED_AND_RETRY) {
                                    pushEventInternal<StopServiceEvent>(stopServiceEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY, stopServiceEvt->serviceId, true);
                                }
                            } else {
                                handleEventCompletion(stopServiceEvt);
                            }
                        } else {
                            pushEventInternal<DependencyOfflineEvent>(toStopService->serviceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY);
                            pushEventInternal<StopServiceEvent>(stopServiceEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY, stopServiceEvt->serviceId, true);
                        }
                    }
                        break;
                    case RemoveServiceEvent::TYPE: {
                        INTERNAL_DEBUG("RemoveServiceEvent");
                        auto removeServiceEvt = static_cast<RemoveServiceEvent *>(evtNode.mapped().get());

                        auto toRemoveServiceIt = _services.find(removeServiceEvt->serviceId);

                        if (toRemoveServiceIt == end(_services)) {
                            ICHOR_LOG_ERROR(_logger, "Couldn't remove service {}, missing from known services", removeServiceEvt->serviceId);
                            handleEventError(removeServiceEvt);
                            break;
                        }

                        auto &toRemoveService = toRemoveServiceIt->second;
                        if (removeServiceEvt->dependenciesStopped) {
                            auto ret = toRemoveService->stop();
                            if (toRemoveService->getServiceState() == ServiceState::ACTIVE && ret != StartBehaviour::SUCCEEDED) {
                                ICHOR_LOG_ERROR(_logger, "Couldn't remove service {}: {} but all dependencies stopped", removeServiceEvt->serviceId,
                                          toRemoveService->implementationName());
                                handleEventError(removeServiceEvt);
                                if(ret == StartBehaviour::FAILED_AND_RETRY) {
                                    pushEventInternal<RemoveServiceEvent>(removeServiceEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY, removeServiceEvt->serviceId,
                                                                          true);
                                }
                            } else {
                                handleEventCompletion(removeServiceEvt);
                                _services.erase(toRemoveServiceIt);
                            }
                        } else {
                            pushEventInternal<DependencyOfflineEvent>(toRemoveService->serviceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY);
                            pushEventInternal<RemoveServiceEvent>(removeServiceEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY, removeServiceEvt->serviceId,
                                                                  true);
                        }
                    }
                        break;
                    case StartServiceEvent::TYPE: {
                        INTERNAL_DEBUG("StartServiceEvent");
                        auto startServiceEvt = static_cast<StartServiceEvent *>(evtNode.mapped().get());

                        auto toStartServiceIt = _services.find(startServiceEvt->serviceId);

                        if (toStartServiceIt == end(_services)) {
                            ICHOR_LOG_ERROR(_logger, "Couldn't start service {}, missing from known services", startServiceEvt->serviceId);
                            handleEventError(startServiceEvt);
                            break;
                        }

                        auto &toStartService = toStartServiceIt->second;
                        if (toStartService->getServiceState() == ServiceState::ACTIVE) {
                            handleEventCompletion(startServiceEvt);
                        } else {
                            auto ret = toStartService->start();
                            if (ret == StartBehaviour::SUCCEEDED) {
                                pushEventInternal<DependencyOnlineEvent>(toStartService->serviceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY);
                                handleEventCompletion(startServiceEvt);
                            } else {
                                INTERNAL_DEBUG("Couldn't start service {}: {}", startServiceEvt->serviceId, toStartService->implementationName());
                                handleEventError(startServiceEvt);
                                if(ret == StartBehaviour::FAILED_AND_RETRY) {
                                    pushEventInternal<StartServiceEvent>(startServiceEvt->originatingService, INTERNAL_DEPENDENCY_EVENT_PRIORITY, startServiceEvt->serviceId);
                                }
                            }
                        }
                    }
                        break;
                    case DoWorkEvent::TYPE: {
                        INTERNAL_DEBUG("DoWorkEvent");
                        handleEventCompletion(evtNode.mapped().get());
                    }
                        break;
                    case RemoveCompletionCallbacksEvent::TYPE: {
                        INTERNAL_DEBUG("RemoveCompletionCallbacksEvent");
                        auto removeCallbacksEvt = static_cast<RemoveCompletionCallbacksEvent *>(evtNode.mapped().get());

                        _completionCallbacks.erase(removeCallbacksEvt->key);
                        _errorCallbacks.erase(removeCallbacksEvt->key);
                    }
                        break;
                    case RemoveEventHandlerEvent::TYPE: {
                        INTERNAL_DEBUG("RemoveEventHandlerEvent");
                        auto removeEventHandlerEvt = static_cast<RemoveEventHandlerEvent *>(evtNode.mapped().get());

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
                        INTERNAL_DEBUG("RemoveEventInterceptorEvent");
                        auto removeEventHandlerEvt = static_cast<RemoveEventInterceptorEvent *>(evtNode.mapped().get());

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
                        INTERNAL_DEBUG("RemoveTrackerEvent");
                        auto removeTrackerEvt = static_cast<RemoveTrackerEvent *>(evtNode.mapped().get());

                        _dependencyRequestTrackers.erase(removeTrackerEvt->interfaceNameHash);
                        _dependencyUndoRequestTrackers.erase(removeTrackerEvt->interfaceNameHash);
                    }
                        break;
                    case ContinuableEvent<Generator<bool>>::TYPE: {
                        INTERNAL_DEBUG("ContinuableEvent");
                        auto continuableEvt = static_cast<ContinuableEvent<Generator<bool>> *>(evtNode.mapped().get());

                        auto it = continuableEvt->generator.begin();

                        if (it != continuableEvt->generator.end()) {
                            pushEventInternal<ContinuableEvent<Generator<bool>>>(continuableEvt->originatingService, evtNode.key(), std::move(continuableEvt->generator));
                        }
                    }
                        break;
                    case RunFunctionEvent::TYPE: {
                        INTERNAL_DEBUG("RunFunctionEvent");
                        auto runFunctionEvt = static_cast<RunFunctionEvent *>(evtNode.mapped().get());
                        runFunctionEvt->fun(this);
                    }
                        break;
                    default: {
                        INTERNAL_DEBUG("broadcastEvent");
                        handlerAmount = broadcastEvent(evtNode.mapped().get());
                    }
                        break;
                }
            }

            if(interceptorsForAllEvents != end(_eventInterceptors)) {
                for(EventInterceptInfo &info : interceptorsForAllEvents->second) {
                    info.postIntercept(evtNode.mapped().get(), allowProcessing && handlerAmount > 0);
                }
            }

            if(interceptorsForEvent != end(_eventInterceptors)) {
                for(EventInterceptInfo &info : interceptorsForEvent->second) {
                    info.postIntercept(evtNode.mapped().get(), allowProcessing && handlerAmount > 0);
                }
            }

            lck.lock();
        }

        _emptyQueue = true;

        if(!_quit.load(std::memory_order_acquire)) {
            _wakeUp.wait_for(lck, std::chrono::milliseconds(1), [this] { return !_eventQueue.empty(); });
        }

        lck.unlock();
    }

    for(auto &[key, manager] : _services) {
        manager->stop();
    }

    _services.clear();
    _eventQueue.clear();

    if(_communicationChannel != nullptr) {
        _communicationChannel->removeManager(this);
    }

    _started = false;
}

void Ichor::DependencyManager::handleEventCompletion(const Ichor::Event *const evt) {
    if(evt->originatingService == 0) {
        return;
    }

    auto service = _services.find(evt->originatingService);
    if(service == end(_services) || (service->second->getServiceState() != ServiceState::ACTIVE && service->second->getServiceState() != ServiceState::INJECTING)) {
        return;
    }

    auto callback = _completionCallbacks.find(CallbackKey{evt->originatingService, evt->type});
    if(callback == end(_completionCallbacks)) {
        return;
    }

    callback->second(evt);
}

uint32_t Ichor::DependencyManager::broadcastEvent(const Ichor::Event *const evt) {
    auto registeredListeners = _eventCallbacks.find(evt->type);
    if(registeredListeners == end(_eventCallbacks)) {
        return 0;
    }

    for(auto &callbackInfo : registeredListeners->second) {
        auto service = _services.find(callbackInfo.listeningServiceId);
        if(service == end(_services) || (service->second->getServiceState() != ServiceState::ACTIVE && service->second->getServiceState() != ServiceState::INJECTING)) {
            continue;
        }

        if(callbackInfo.filterServiceId.has_value() && *callbackInfo.filterServiceId != evt->originatingService) {
            continue;
        }

        bool allowOtherHandlers;
        auto ret = callbackInfo.callback(evt);
        auto it = ret.begin();

        allowOtherHandlers = *it;
        if(it != ret.end()) {
            pushEventInternal<ContinuableEvent<Generator<bool>>>(evt->originatingService, evt->priority, std::move(ret));
        }

        if(!allowOtherHandlers) {
            break;
        }
    }

    return registeredListeners->second.size();
}

std::optional<std::string_view> Ichor::DependencyManager::getImplementationNameFor(uint64_t serviceId) const noexcept {
    auto service = _services.find(serviceId);

    if(service == end(_services)) {
        return {};
    }

    return service->second->implementationName();
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