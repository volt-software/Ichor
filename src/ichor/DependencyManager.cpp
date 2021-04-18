#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO

#include <ichor/DependencyManager.h>
#include <ichor/CommunicationChannel.h>
#include <ichor/stl/Any.h>
#include <ichor/GetThreadLocalMemoryResource.h>

#if !__has_include(<spdlog/spdlog.h>)
#define SPDLOG_DEBUG(x)
#else
#include <spdlog/spdlog.h>
#endif

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

    ICHOR_LOG_DEBUG(_logger, "starting dm");

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
            auto interceptorsForEvent = _eventInterceptors.find(evtNode.mapped().getType());

            if(interceptorsForAllEvents != end(_eventInterceptors)) {
                for(const EventInterceptInfo &info : interceptorsForAllEvents->second) {
                    if(!info.preIntercept(evtNode.mapped().get())) {
                        allowProcessing = false;
                    }
                }
            }

            if(interceptorsForEvent != end(_eventInterceptors)) {
                for(const EventInterceptInfo &info : interceptorsForEvent->second) {
                    if(!info.preIntercept(evtNode.mapped().get())) {
                        allowProcessing = false;
                    }
                }
            }

            if(allowProcessing) {
                switch (evtNode.mapped().getType()) {
                    case DependencyOnlineEvent::TYPE: {
                        SPDLOG_DEBUG("DependencyOnlineEvent");
                        auto depOnlineEvt = static_cast<DependencyOnlineEvent *>(evtNode.mapped().get());
                        auto managerIt = _services.find(depOnlineEvt->originatingService);

                        if(managerIt == end(_services)) {
                            break;
                        }

                        auto &manager = managerIt->second;

                        auto filterProp = manager->getProperties()->find("Filter");
                        const Filter *filter = nullptr;
                        if (filterProp != end(*manager->getProperties())) {
                            filter = Ichor::any_cast<Filter * const>(&filterProp->second);
                        }

                        std::pmr::vector<ILifecycleManager*> interestedManagers{_memResource};
                        interestedManagers.reserve(_services.size());
                        for (auto const &[key, possibleDependentLifecycleManager] : _services) {
                            if (filter != nullptr && !filter->compareTo(possibleDependentLifecycleManager)) {
                                continue;
                            }

                            if(possibleDependentLifecycleManager->dependencyOnline(manager.get())) {
                                interestedManagers.emplace_back(possibleDependentLifecycleManager.get());
                            }
                        }

                        for(auto const &interestedManager : interestedManagers) {
                            if (interestedManager->start()) {
                                pushEventInternal<DependencyOnlineEvent>(interestedManager->serviceId(), interestedManager->getPriority());
                            }
                        }
                    }
                        break;
                    case DependencyOfflineEvent::TYPE: {
                        SPDLOG_DEBUG("DependencyOfflineEvent");
                        auto depOfflineEvt = static_cast<DependencyOfflineEvent *>(evtNode.mapped().get());
                        auto managerIt = _services.find(depOfflineEvt->originatingService);

                        if(managerIt == end(_services)) {
                            break;
                        }

                        auto &manager = managerIt->second;

                        auto filterProp = manager->getProperties()->find("Filter");
                        const Filter *filter = nullptr;
                        if (filterProp != end(*manager->getProperties())) {
                            filter = Ichor::any_cast<Filter * const>(&filterProp->second);
                        }

                        std::pmr::vector<ILifecycleManager*> interestedManagers{_memResource};
                        interestedManagers.reserve(_services.size());
                        for (auto const &[key, possibleDependentLifecycleManager] : _services) {
                            if (filter != nullptr && !filter->compareTo(possibleDependentLifecycleManager)) {
                                continue;
                            }

                            if(possibleDependentLifecycleManager->dependencyOffline(manager.get())) {
                                interestedManagers.emplace_back(possibleDependentLifecycleManager.get());
                            }
                        }

                        for(auto const &interestedManager : interestedManagers) {
                            if (interestedManager->stop()) {
                                pushEventInternal<DependencyOfflineEvent>(interestedManager->serviceId(), interestedManager->getPriority());
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

                        for (DependencyTrackerInfo const &info : trackers->second) {
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

                        for (DependencyTrackerInfo const &info : trackers->second) {
                            info.trackFunc(depUndoReqEvt);
                        }
                    }
                        break;
                    case QuitEvent::TYPE: {
                        SPDLOG_DEBUG("QuitEvent");
                        auto _quitEvt = static_cast<QuitEvent *>(evtNode.mapped().get());
                        if (!_quitEvt->dependenciesStopped) {
                            for (auto const &[key, possibleManager] : _services) {
                                pushEventInternal<StopServiceEvent>(_quitEvt->originatingService, possibleManager->getPriority(), possibleManager->serviceId());
                            }

                            pushEventInternal<QuitEvent>(_quitEvt->originatingService, INTERNAL_EVENT_PRIORITY + 1, true);
                        } else {
                            bool canFinally_quit = true;
                            for (auto const &[key, manager] : _services) {
                                if (manager->getServiceState() != ServiceState::INSTALLED) {
                                    canFinally_quit = false;
                                    break;
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
                        SPDLOG_DEBUG("StopServiceEvent");
                        auto stopServiceEvt = static_cast<StopServiceEvent *>(evtNode.mapped().get());

                        auto toStopServiceIt = _services.find(stopServiceEvt->serviceId);

                        if (toStopServiceIt == end(_services)) {
                            ICHOR_LOG_ERROR(_logger, "Couldn't stop service {}, missing from known services", stopServiceEvt->serviceId);
                            handleEventError(stopServiceEvt);
                            break;
                        }

                        auto &toStopService = toStopServiceIt->second;
                        if (stopServiceEvt->dependenciesStopped) {
                            if (toStopService->getServiceState() == ServiceState::ACTIVE && !toStopService->stop()) {
                                ICHOR_LOG_ERROR(_logger, "Couldn't stop service {}: {} but all dependencies stopped", stopServiceEvt->serviceId,
                                          toStopService->implementationName());
                                handleEventError(stopServiceEvt);
                            } else {
                                handleEventCompletion(stopServiceEvt);
                            }
                        } else {
                            pushEventInternal<DependencyOfflineEvent>(toStopService->serviceId(), stopServiceEvt->priority);
                            pushEventInternal<StopServiceEvent>(stopServiceEvt->originatingService, stopServiceEvt->priority, stopServiceEvt->serviceId, true);
                        }
                    }
                        break;
                    case RemoveServiceEvent::TYPE: {
                        SPDLOG_DEBUG("RemoveServiceEvent");
                        auto removeServiceEvt = static_cast<RemoveServiceEvent *>(evtNode.mapped().get());

                        auto toRemoveServiceIt = _services.find(removeServiceEvt->serviceId);

                        if (toRemoveServiceIt == end(_services)) {
                            ICHOR_LOG_ERROR(_logger, "Couldn't remove service {}, missing from known services", removeServiceEvt->serviceId);
                            handleEventError(removeServiceEvt);
                            break;
                        }

                        auto &toRemoveService = toRemoveServiceIt->second;
                        if (removeServiceEvt->dependenciesStopped) {
                            if (toRemoveService->getServiceState() == ServiceState::ACTIVE && !toRemoveService->stop()) {
                                ICHOR_LOG_ERROR(_logger, "Couldn't remove service {}: {} but all dependencies stopped", removeServiceEvt->serviceId,
                                          toRemoveService->implementationName());
                                handleEventError(removeServiceEvt);
                            } else {
                                handleEventCompletion(removeServiceEvt);
                                _services.erase(toRemoveServiceIt);
                            }
                        } else {
                            pushEventInternal<DependencyOfflineEvent>(toRemoveService->serviceId(), removeServiceEvt->priority);
                            pushEventInternal<RemoveServiceEvent>(removeServiceEvt->originatingService, removeServiceEvt->priority, removeServiceEvt->serviceId,
                                                                  true);
                        }
                    }
                        break;
                    case StartServiceEvent::TYPE: {
                        SPDLOG_DEBUG("StartServiceEvent");
                        auto startServiceEvt = static_cast<StartServiceEvent *>(evtNode.mapped().get());

                        auto toStartServiceIt = _services.find(startServiceEvt->serviceId);

                        if (toStartServiceIt == end(_services)) {
                            ICHOR_LOG_ERROR(_logger, "Couldn't start service {}, missing from known services", startServiceEvt->serviceId);
                            handleEventError(startServiceEvt);
                            break;
                        }

                        auto &toStartService = toStartServiceIt->second;
                        if(toStartService->getServiceState() == ServiceState::ACTIVE) {
                            handleEventCompletion(startServiceEvt);
                        } else if (!toStartService->start()) {
//                            ICHOR_LOG_TRACE(_logger, "Couldn't start service {}: {}", startServiceEvt->serviceId, toStartService->implementationName());
                            handleEventError(startServiceEvt);
                        } else {
                            pushEventInternal<DependencyOnlineEvent>(toStartService->serviceId(), startServiceEvt->priority);
                            handleEventCompletion(startServiceEvt);
                        }
                    }
                        break;
                    case DoWorkEvent::TYPE: {
                        SPDLOG_DEBUG("DoWorkEvent");
                        handleEventCompletion(evtNode.mapped().get());
                    }
                        break;
                    case RemoveCompletionCallbacksEvent::TYPE: {
                        SPDLOG_DEBUG("RemoveCompletionCallbacksEvent");
                        auto removeCallbacksEvt = static_cast<RemoveCompletionCallbacksEvent *>(evtNode.mapped().get());

                        _completionCallbacks.erase(removeCallbacksEvt->key);
                        _errorCallbacks.erase(removeCallbacksEvt->key);
                    }
                        break;
                    case RemoveEventHandlerEvent::TYPE: {
                        SPDLOG_DEBUG("RemoveEventHandlerEvent");
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
                        SPDLOG_DEBUG("RemoveEventInterceptorEvent");
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
                        SPDLOG_DEBUG("RemoveTrackerEvent");
                        auto removeTrackerEvt = static_cast<RemoveTrackerEvent *>(evtNode.mapped().get());

                        _dependencyRequestTrackers.erase(removeTrackerEvt->interfaceNameHash);
                        _dependencyUndoRequestTrackers.erase(removeTrackerEvt->interfaceNameHash);
                    }
                        break;
                    case ContinuableEvent<Generator<bool>>::TYPE: {
                        SPDLOG_DEBUG("ContinuableEvent");
                        auto continuableEvt = static_cast<ContinuableEvent<Generator<bool>> *>(evtNode.mapped().get());

                        auto it = continuableEvt->generator.begin();

                        if (it != continuableEvt->generator.end()) {
                            pushEventInternal<ContinuableEvent<Generator<bool>>>(continuableEvt->originatingService, evtNode.key(), std::move(continuableEvt->generator));
                        }
                    }
                        break;
                    case RunFunctionEvent::TYPE: {
                        SPDLOG_DEBUG("RunFunctionEvent");
                        auto runFunctionEvt = static_cast<RunFunctionEvent *>(evtNode.mapped().get());
                        runFunctionEvt->fun(this);
                    }
                        break;
                    default: {
                        SPDLOG_DEBUG("broadcastEvent");
                        handlerAmount = broadcastEvent(evtNode.mapped().get());
                    }
                        break;
                }
            }

            if(interceptorsForAllEvents != end(_eventInterceptors)) {
                for(const EventInterceptInfo &info : interceptorsForAllEvents->second) {
                    info.postIntercept(evtNode.mapped().get(), allowProcessing && handlerAmount > 0);
                }
            }

            if(interceptorsForEvent != end(_eventInterceptors)) {
                for(const EventInterceptInfo &info : interceptorsForEvent->second) {
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

    if(_communicationChannel != nullptr) {
        _communicationChannel->removeManager(this);
    }

    _started = false;
}

void Ichor::DependencyManager::handleEventCompletion(const Ichor::Event *const evt) const  {
    if(evt->originatingService == 0) {
        return;
    }

    auto service = _services.find(evt->originatingService);
    if(service == end(_services) || service->second->getServiceState() != ServiceState::ACTIVE) {
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
        if(service == end(_services) || service->second->getServiceState() != ServiceState::ACTIVE) {
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

Ichor::EventHandlerRegistration::~EventHandlerRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushPrioritisedEvent<RemoveEventHandlerEvent>(0, _priority, _key);
    }
}

Ichor::EventInterceptorRegistration::~EventInterceptorRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushPrioritisedEvent<RemoveEventInterceptorEvent>(0, _priority, _key);
    }
}

Ichor::DependencyTrackerRegistration::~DependencyTrackerRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushPrioritisedEvent<RemoveTrackerEvent>(0, _priority, _interfaceNameHash);
    }
}


// ugly hack to catch the place where an exception is thrown
// boost asio/beast are absolutely terrible when it comes to debugging these things
// only works on gcc, probably. See https://stackoverflow.com/a/11674810/1460998
#if USE_UGLY_HACK_EXCEPTION_CATCHING
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