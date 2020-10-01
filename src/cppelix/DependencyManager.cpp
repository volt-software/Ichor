#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO

#include <cppelix/DependencyManager.h>
#include <cppelix/Callback.h>
#include <cppelix/CommunicationChannel.h>

#if !__has_include(<spdlog/spdlog.h>)
#define SPDLOG_DEBUG(x)
#else
#include <spdlog/spdlog.h>
#endif

std::atomic<bool> sigintQuit;
std::atomic<uint64_t> Cppelix::DependencyManager::_managerIdCounter = 0;

void on_sigint([[maybe_unused]] int sig) {
    sigintQuit.store(true, std::memory_order_release);
}

void Cppelix::DependencyManager::start() {
    assert(_logger != nullptr);
    LOG_DEBUG(_logger, "starting dm");

    ::signal(SIGINT, on_sigint);

#ifdef __linux__
    pthread_setname_np(pthread_self(), fmt::format("DepMan #{}", _id).c_str());
#endif

    while(!_quit.load(std::memory_order_acquire)) {
        _quit.store(sigintQuit.load(std::memory_order_acquire), std::memory_order_release);
        std::unique_lock lck(_eventQueueMutex);
        while (!_quit.load(std::memory_order_acquire) && !_eventQueue.empty()) {
            auto evtNode = _eventQueue.extract(_eventQueue.begin());
            lck.unlock();
            _quit.store(sigintQuit.load(std::memory_order_acquire), std::memory_order_release);

            bool allowProcessing = true;
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

                        auto filterProp = depOnlineEvt->manager->getProperties()->find("Filter");
                        const Filter *filter = nullptr;
                        if (filterProp != end(*depOnlineEvt->manager->getProperties())) {
                            filter = std::any_cast<const Filter>(&filterProp->second);
                        }

                        for (auto &[key, possibleDependentLifecycleManager] : _services) {
                            if (filter != nullptr && !filter->compareTo(possibleDependentLifecycleManager)) {
                                continue;
                            }

                            if (possibleDependentLifecycleManager->dependencyOnline(depOnlineEvt->manager)) {
                                pushEventInternal<DependencyOnlineEvent>(0, INTERNAL_EVENT_PRIORITY, possibleDependentLifecycleManager);
                            }
                        }
                    }
                        break;
                    case DependencyOfflineEvent::TYPE: {
                        SPDLOG_DEBUG("DependencyOfflineEvent");
                        auto depOfflineEvt = static_cast<DependencyOfflineEvent *>(evtNode.mapped().get());

                        auto filterProp = depOfflineEvt->manager->getProperties()->find("Filter");
                        const Filter *filter = nullptr;
                        if (filterProp != end(*depOfflineEvt->manager->getProperties())) {
                            filter = std::any_cast<const Filter>(&filterProp->second);
                        }

                        for (auto &[key, possibleDependentLifecycleManager] : _services) {
                            if (filter != nullptr && !filter->compareTo(possibleDependentLifecycleManager)) {
                                continue;
                            }

                            if (possibleDependentLifecycleManager->dependencyOffline(depOfflineEvt->manager)) {
                                pushEventInternal<DependencyOfflineEvent>(0, INTERNAL_EVENT_PRIORITY, possibleDependentLifecycleManager);
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
                        SPDLOG_DEBUG("QuitEvent");
                        auto _quitEvt = static_cast<QuitEvent *>(evtNode.mapped().get());
                        if (!_quitEvt->dependenciesStopped) {
                            for (auto &[key, possibleManager] : _services) {
                                pushEventInternal<StopServiceEvent>(_quitEvt->originatingService, INTERNAL_EVENT_PRIORITY, possibleManager->serviceId());
                            }

                            pushEventInternal<QuitEvent>(_quitEvt->originatingService, INTERNAL_EVENT_PRIORITY + 1, true);
                        } else {
                            bool canFinally_quit = true;
                            for (auto &[key, manager] : _services) {
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
                            LOG_ERROR(_logger, "Couldn't stop service {}, missing from known services", stopServiceEvt->serviceId);
                            handleEventError(stopServiceEvt);
                            break;
                        }

                        auto &toStopService = toStopServiceIt->second;
                        if (stopServiceEvt->dependenciesStopped) {
                            if (toStopService->getServiceState() == ServiceState::ACTIVE && !toStopService->stop()) {
                                LOG_ERROR(_logger, "Couldn't stop service {}: {} but all dependencies stopped", stopServiceEvt->serviceId,
                                          toStopService->implementationName());
                                handleEventError(stopServiceEvt);
                            } else {
                                handleEventCompletion(stopServiceEvt);
                            }
                        } else {
                            pushEventInternal<DependencyOfflineEvent>(0, INTERNAL_EVENT_PRIORITY, toStopService);
                            pushEventInternal<StopServiceEvent>(stopServiceEvt->originatingService, INTERNAL_EVENT_PRIORITY, stopServiceEvt->serviceId, true);
                        }
                    }
                        break;
                    case RemoveServiceEvent::TYPE: {
                        SPDLOG_DEBUG("RemoveServiceEvent");
                        auto removeServiceEvt = static_cast<RemoveServiceEvent *>(evtNode.mapped().get());

                        auto toRemoveServiceIt = _services.find(removeServiceEvt->serviceId);

                        if (toRemoveServiceIt == end(_services)) {
                            LOG_ERROR(_logger, "Couldn't remove service {}, missing from known services", removeServiceEvt->serviceId);
                            handleEventError(removeServiceEvt);
                            break;
                        }

                        auto &toRemoveService = toRemoveServiceIt->second;
                        if (removeServiceEvt->dependenciesStopped) {
                            if (toRemoveService->getServiceState() == ServiceState::ACTIVE && !toRemoveService->stop()) {
                                LOG_ERROR(_logger, "Couldn't remove service {}: {} but all dependencies stopped", removeServiceEvt->serviceId,
                                          toRemoveService->implementationName());
                                handleEventError(removeServiceEvt);
                            } else {
                                handleEventCompletion(removeServiceEvt);
                                _services.erase(toRemoveServiceIt);
                            }
                        } else {
                            pushEventInternal<DependencyOfflineEvent>(0, INTERNAL_EVENT_PRIORITY, toRemoveService);
                            pushEventInternal<RemoveServiceEvent>(removeServiceEvt->originatingService, INTERNAL_EVENT_PRIORITY, removeServiceEvt->serviceId,
                                                                  true);
                        }
                    }
                        break;
                    case StartServiceEvent::TYPE: {
                        SPDLOG_DEBUG("StartServiceEvent");
                        auto startServiceEvt = static_cast<StartServiceEvent *>(evtNode.mapped().get());

                        auto toStartServiceIt = _services.find(startServiceEvt->serviceId);

                        if (toStartServiceIt == end(_services)) {
                            LOG_ERROR(_logger, "Couldn't start service {}, missing from known services", startServiceEvt->serviceId);
                            handleEventError(startServiceEvt);
                            break;
                        }

                        auto &toStartService = toStartServiceIt->second;
                        if(toStartService->getServiceState() == ServiceState::ACTIVE) {
                            handleEventCompletion(startServiceEvt);
                        } else if (!toStartService->start()) {
//                            LOG_TRACE(_logger, "Couldn't start service {}: {}", startServiceEvt->serviceId, toStartService->implementationName());
                            handleEventError(startServiceEvt);
                        } else {
                            pushEventInternal<DependencyOnlineEvent>(0, INTERNAL_EVENT_PRIORITY, toStartService);
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
                    case ContinuableEvent::TYPE: {
                        SPDLOG_DEBUG("ContinuableEvent");
                        auto continuableEvt = static_cast<ContinuableEvent *>(evtNode.mapped().get());

                        auto it = continuableEvt->generator.begin();

                        if (it != continuableEvt->generator.end()) {
                            pushEventInternal<ContinuableEvent>(continuableEvt->originatingService, evtNode.key(), std::move(continuableEvt->generator));
                        }
                    }
                        break;
                    default: {
                        SPDLOG_DEBUG("broadcastEvent");
                        broadcastEvent(evtNode.mapped().get());
                    }
                        break;
                }
            }

            if(interceptorsForAllEvents != end(_eventInterceptors)) {
                for(const EventInterceptInfo &info : interceptorsForAllEvents->second) {
                    info.postIntercept(evtNode.mapped().get(), allowProcessing);
                }
            }

            if(interceptorsForEvent != end(_eventInterceptors)) {
                for(const EventInterceptInfo &info : interceptorsForEvent->second) {
                    info.postIntercept(evtNode.mapped().get(), allowProcessing);
                }
            }

            lck.lock();
        }

        _wakeUp.wait_for(lck, std::chrono::milliseconds(1), [this]{return !_eventQueue.empty(); });

        lck.unlock();
    }

    for(auto &[key, manager] : _services) {
        manager->stop();
    }

    _services.clear();

    if(_communicationChannel != nullptr) {
        _communicationChannel->removeManager(this);
    }
}

void Cppelix::DependencyManager::handleEventCompletion(const Cppelix::Event *const evt) const  {
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

void Cppelix::DependencyManager::broadcastEvent(const Cppelix::Event *const evt) {
    auto registeredListeners = _eventCallbacks.find(evt->type);
    if(registeredListeners == end(_eventCallbacks)) {
        return;
    }

    for(auto &callbackInfo : registeredListeners->second) {
        auto service = _services.find(callbackInfo.listeningServiceId);
        if(service == end(_services) || service->second->getServiceState() != ServiceState::ACTIVE) {
            continue;
        }

        if(callbackInfo.filterServiceId.has_value() && *callbackInfo.filterServiceId != evt->originatingService) {
            continue;
        }

        auto ret = callbackInfo.callback(evt);
        auto it = ret.begin();

        bool allowOtherHandlers = *it;
        if(it != ret.end()) {
            pushEventInternal<ContinuableEvent>(evt->originatingService, evt->priority, std::move(ret));
        }

        if(!allowOtherHandlers) {
            break;
        }
    }
}

std::optional<std::string_view> Cppelix::DependencyManager::getImplementationNameFor(uint64_t serviceId) {
    auto service = _services.find(serviceId);

    if(service == end(_services)) {
        return {};
    }

    return service->second->implementationName();
}

void Cppelix::DependencyManager::setCommunicationChannel(Cppelix::CommunicationChannel *channel) {
    _communicationChannel = channel;
}

Cppelix::EventCompletionHandlerRegistration::~EventCompletionHandlerRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushEvent<RemoveCompletionCallbacksEvent>(0, _key);
    }
}

Cppelix::EventHandlerRegistration::~EventHandlerRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushEvent<RemoveEventHandlerEvent>(0, _key);
    }
}

Cppelix::EventInterceptorRegistration::~EventInterceptorRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushEvent<RemoveEventInterceptorEvent>(0, _key);
    }
}

Cppelix::DependencyTrackerRegistration::~DependencyTrackerRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushEvent<RemoveTrackerEvent>(0, _interfaceNameHash);
    }
}
