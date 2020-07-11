#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO

#include "framework/DependencyManager.h"
#include "framework/Callback.h"

std::atomic<bool> sigintQuit;

void on_sigint([[maybe_unused]] int sig) {
    sigintQuit.store(true, std::memory_order_release);
}

void Cppelix::DependencyManager::start() {
    assert(_logger != nullptr);
    LOG_DEBUG(_logger, "starting dm");

    ::signal(SIGINT, on_sigint);

    while(!quit.load(std::memory_order_acquire)) {
        EventStackUniquePtr evt{};
        quit.store(sigintQuit.load(std::memory_order_acquire), std::memory_order_release);
        while (!quit.load(std::memory_order_acquire) && _eventQueue.try_dequeue(_consumerToken, evt)) {
            quit.store(sigintQuit.load(std::memory_order_acquire), std::memory_order_release);
            switch(evt.getType()) {
                case DependencyOnlineEvent::TYPE: {
                    SPDLOG_DEBUG("DependencyOnlineEvent");
                    auto depOnlineEvt = static_cast<DependencyOnlineEvent *>(evt.get());

                    auto filterProp = depOnlineEvt->manager->getProperties()->find("Filter");
                    const Filter *filter = nullptr;
                    if(filterProp != end(*depOnlineEvt->manager->getProperties())) {
                        filter = std::any_cast<const Filter>(&filterProp->second);
                    }

                    for (auto &[key, possibleDependentLifecycleManager] : _services) {
                        if(filter != nullptr && !filter->compareTo(possibleDependentLifecycleManager)) {
                            continue;
                        }

                        if (possibleDependentLifecycleManager->dependencyOnline(depOnlineEvt->manager)) {
                            _eventQueue.enqueue(_producerToken, EventStackUniquePtr::create<DependencyOnlineEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, possibleDependentLifecycleManager));
                        }
                    }
                }
                    break;
                case DependencyOfflineEvent::TYPE: {
                    SPDLOG_DEBUG("DependencyOfflineEvent");
                    auto depOfflineEvt = static_cast<DependencyOfflineEvent *>(evt.get());

                    auto filterProp = depOfflineEvt->manager->getProperties()->find("Filter");
                    const Filter *filter = nullptr;
                    if(filterProp != end(*depOfflineEvt->manager->getProperties())) {
                        filter = std::any_cast<const Filter>(&filterProp->second);
                    }

                    for (auto &[key, possibleDependentLifecycleManager] : _services) {
                        if(filter != nullptr && !filter->compareTo(possibleDependentLifecycleManager)) {
                            continue;
                        }

                        if (possibleDependentLifecycleManager->dependencyOffline(depOfflineEvt->manager)) {
                            _eventQueue.enqueue(_producerToken, EventStackUniquePtr::create<DependencyOfflineEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, possibleDependentLifecycleManager));
                        }
                    }
                }
                    break;
                case DependencyRequestEvent::TYPE: {
                    auto depReqEvt = static_cast<DependencyRequestEvent *>(evt.get());

                    auto trackers = _dependencyRequestTrackers.find(depReqEvt->dependency.interfaceNameHash);
                    if(trackers == end(_dependencyRequestTrackers)) {
                        break;
                    }

                    for(DependencyTrackerInfo &info : trackers->second) {
                        info.trackFunc(depReqEvt);
                    }
                }
                    break;
                case DependencyUndoRequestEvent::TYPE: {
                    auto depUndoReqEvt = static_cast<DependencyUndoRequestEvent *>(evt.get());

                    auto trackers = _dependencyUndoRequestTrackers.find(depUndoReqEvt->dependency.interfaceNameHash);
                    if(trackers == end(_dependencyUndoRequestTrackers)) {
                        break;
                    }

                    for(DependencyTrackerInfo &info : trackers->second) {
                        info.trackFunc(depUndoReqEvt);
                    }
                }
                    break;
                case QuitEvent::TYPE: {
                    SPDLOG_DEBUG("QuitEvent");
                    auto quitEvt = static_cast<QuitEvent *>(evt.get());
                    if(!quitEvt->dependenciesStopped) {
                        for(auto &[key, possibleManager] : _services) {
                            _eventQueue.enqueue(_producerToken, EventStackUniquePtr::create<StopServiceEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), quitEvt->originatingService, possibleManager->serviceId()));
                        }

                        _eventQueue.enqueue(_producerToken, EventStackUniquePtr::create<QuitEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), quitEvt->originatingService, true));
                    } else {
                        bool canFinallyQuit = true;
                        for(auto &[key, manager] : _services) {
                            if(manager->getServiceState() != ServiceState::INSTALLED) {
                                canFinallyQuit = false;
                                break;
                            }
                        }

                        if(canFinallyQuit) {
                            quit.store(true, std::memory_order_release);
                        } else {
                            _eventQueue.enqueue(_producerToken, EventStackUniquePtr::create<QuitEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), quitEvt->originatingService, false));
                        }
                    }
                }
                    break;
                case StopServiceEvent::TYPE: {
                    SPDLOG_DEBUG("StopServiceEvent");
                    auto stopServiceEvt = static_cast<StopServiceEvent *>(evt.get());

                    auto toStopServiceIt = _services.find(stopServiceEvt->serviceId);

                    if(toStopServiceIt == end(_services)) {
                        LOG_ERROR(_logger, "Couldn't stop service {}, missing from known services", stopServiceEvt->serviceId);
                        handleEventError(stopServiceEvt);
                        break;
                    }

                    auto &toStopService = toStopServiceIt->second;
                    if(stopServiceEvt->dependenciesStopped) {
                        if(toStopService->getServiceState() == ServiceState::ACTIVE && !toStopService->stop()) {
                            LOG_ERROR(_logger, "Couldn't stop service {}: {} but all dependencies stopped", stopServiceEvt->serviceId, toStopService->name());
                            handleEventError(stopServiceEvt);
                        } else {
                            handleEventCompletion(stopServiceEvt);
                        }
                    } else {
                        _eventQueue.enqueue(_producerToken, EventStackUniquePtr::create<DependencyOfflineEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, toStopService));
                        _eventQueue.enqueue(_producerToken, EventStackUniquePtr::create<StopServiceEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), stopServiceEvt->originatingService, stopServiceEvt->serviceId, true));
                    }
                }
                    break;
                case RemoveServiceEvent::TYPE: {
                    SPDLOG_DEBUG("RemoveServiceEvent");
                    auto removeServiceEvt = static_cast<RemoveServiceEvent *>(evt.get());

                    auto toRemoveServiceIt = _services.find(removeServiceEvt->serviceId);

                    if(toRemoveServiceIt == end(_services)) {
                        LOG_ERROR(_logger, "Couldn't stop service {}, missing from known services", removeServiceEvt->serviceId);
                        handleEventError(removeServiceEvt);
                        break;
                    }

                    auto &toRemoveService = toRemoveServiceIt->second;
                    if(removeServiceEvt->dependenciesStopped) {
                        if(toRemoveService->getServiceState() == ServiceState::ACTIVE && !toRemoveService->stop()) {
                            LOG_ERROR(_logger, "Couldn't stop service {}: {} but all dependencies stopped", removeServiceEvt->serviceId, toRemoveService->name());
                            handleEventError(removeServiceEvt);
                        } else {
                            handleEventCompletion(removeServiceEvt);
                        }
                    } else {
                        _eventQueue.enqueue(_producerToken, EventStackUniquePtr::create<DependencyOfflineEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, toRemoveService));
                        _eventQueue.enqueue(_producerToken, EventStackUniquePtr::create<StopServiceEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), removeServiceEvt->originatingService, removeServiceEvt->serviceId, true));
                    }
                }
                    break;
                case StartServiceEvent::TYPE: {
                    SPDLOG_DEBUG("StartServiceEvent");
                    auto startServiceEvt = static_cast<StartServiceEvent *>(evt.get());

                    auto toStartServiceIt = _services.find(startServiceEvt->serviceId);

                    if(toStartServiceIt == end(_services)) {
                        LOG_ERROR(_logger, "Couldn't start service {}, missing from known services", startServiceEvt->serviceId);
                        handleEventError(startServiceEvt);
                        break;
                    }

                    auto &toStartService = toStartServiceIt->second;
                    if(!toStartService->start()) {
                        LOG_ERROR(_logger, "Couldn't start service {}: {}", startServiceEvt->serviceId, toStartService->name());
                        handleEventError(startServiceEvt);
                    } else {
                        _eventQueue.enqueue(_producerToken, EventStackUniquePtr::create<DependencyOnlineEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, toStartService));
                        handleEventCompletion(startServiceEvt);
                    }
                }
                    break;
                case DoWorkEvent::TYPE: {
                    SPDLOG_DEBUG("DoWorkEvent");
                    handleEventCompletion(evt.get());
                }
                    break;
                case RemoveCompletionCallbacksEvent::TYPE: {
                    SPDLOG_DEBUG("RemoveCompletionCallbacksEvent");
                    auto removeCallbacksEvt = static_cast<RemoveCompletionCallbacksEvent *>(evt.get());

                    _completionCallbacks.erase(removeCallbacksEvt->key);
                    _errorCallbacks.erase(removeCallbacksEvt->key);
                }
                    break;
                case RemoveEventHandlerEvent::TYPE: {
                    SPDLOG_DEBUG("RemoveEventHandlerEvent");
                    auto removeEventHandlerEvt = static_cast<RemoveEventHandlerEvent *>(evt.get());

                    auto existingHandlers = _eventCallbacks.find(removeEventHandlerEvt->key.type);
                    if(existingHandlers != end(_eventCallbacks)) {
                        std::erase_if(existingHandlers->second, [removeEventHandlerEvt](const auto &pair) noexcept { return pair.first == removeEventHandlerEvt->key.id; });
                    }
                }
                    break;
                case RemoveTrackerEvent::TYPE: {
                    SPDLOG_DEBUG("RemoveTrackerEvent");
                    auto removeTrackerEvt = static_cast<RemoveTrackerEvent *>(evt.get());

                    _dependencyRequestTrackers.erase(removeTrackerEvt->interfaceNameHash);
                    _dependencyUndoRequestTrackers.erase(removeTrackerEvt->interfaceNameHash);
                }
                    break;
                default: {
                    broadcastEvent(evt.get());
                }
                    break;
            }
        }

        std::this_thread::sleep_for(1ms);
    }

    for(auto &[key, manager] : _services) {
        manager->stop();
    }
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

Cppelix::DependencyTrackerRegistration::~DependencyTrackerRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushEvent<RemoveTrackerEvent>(0, _interfaceNameHash);
    }
}
