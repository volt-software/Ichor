#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO

#include "framework/DependencyManager.h"
#include "framework/Callback.h"

std::atomic<bool> quit{false};

void on_sigint([[maybe_unused]] int sig) {
    quit.store(true, std::memory_order_release);
}

void Cppelix::DependencyManager::start() {
    assert(_logger != nullptr);
    LOG_DEBUG(_logger, "starting dm");

    ::signal(SIGINT, on_sigint);

    while(!quit.load(std::memory_order_acquire)) {
        std::unique_ptr<Event> evt{nullptr};
        while (!quit.load(std::memory_order_acquire) && _eventQueue.try_dequeue(_consumerToken, evt)) {
            switch(evt->type) {
                case DependencyOnlineEvent::TYPE: {
                    SPDLOG_DEBUG("DependencyOnlineEvent");
                    auto depOnlineEvt = static_cast<DependencyOnlineEvent *>(evt.get());
                    for (auto &[key, possibleDependentLifecycleManager] : _services) {
                        possibleDependentLifecycleManager->dependencyOnline(depOnlineEvt->manager);

                        if (possibleDependentLifecycleManager->shouldStart()) {
                            if (possibleDependentLifecycleManager->start()) {
                                LOG_DEBUG(_logger, "Started {}", possibleDependentLifecycleManager->name());
                                _eventQueue.enqueue(_producerToken, std::make_unique<DependencyOnlineEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, possibleDependentLifecycleManager));
                            } else {
                                LOG_DEBUG(_logger, "Couldn't start {}", possibleDependentLifecycleManager->name());
                            }
                        }
                    }
                }
                    break;
                case DependencyOfflineEvent::TYPE: {
                    SPDLOG_DEBUG("DependencyOfflineEvent");
                    auto depOfflineEvt = static_cast<DependencyOfflineEvent *>(evt.get());
                    for (auto &[key, possibleDependentLifecycleManager] : _services) {
                        possibleDependentLifecycleManager->dependencyOffline(depOfflineEvt->manager);

                        if (possibleDependentLifecycleManager->shouldStop()) {
                            if (possibleDependentLifecycleManager->stop()) {
                                LOG_DEBUG(_logger, "stopped {}", possibleDependentLifecycleManager->name());
                                _eventQueue.enqueue(_producerToken, std::make_unique<DependencyOfflineEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, possibleDependentLifecycleManager));
                            } else {
                                LOG_DEBUG(_logger, "Couldn't stop {}", possibleDependentLifecycleManager->name());
                            }
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
                            _eventQueue.enqueue(_producerToken, std::make_unique<StopServiceEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), quitEvt->originatingService, possibleManager->serviceId()));
                        }

                        _eventQueue.enqueue(_producerToken, std::make_unique<QuitEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), quitEvt->originatingService, true));
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
                            _eventQueue.enqueue(_producerToken, std::make_unique<QuitEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), quitEvt->originatingService, false));
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
                        _eventQueue.enqueue(_producerToken, std::make_unique<DependencyOfflineEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, toStopService));
                        _eventQueue.enqueue(_producerToken, std::make_unique<StopServiceEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), stopServiceEvt->originatingService, stopServiceEvt->serviceId, true));
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
                        _eventQueue.enqueue(_producerToken, std::make_unique<DependencyOfflineEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, toRemoveService));
                        _eventQueue.enqueue(_producerToken, std::make_unique<StopServiceEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), removeServiceEvt->originatingService, removeServiceEvt->serviceId, true));
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
                    if(toStartService->shouldStart()) {
                        if(!toStartService->start()) {
                            LOG_ERROR(_logger, "Couldn't start service {}: {}", startServiceEvt->serviceId, toStartService->name());
                            handleEventError(startServiceEvt);
                        } else {
                            _eventQueue.enqueue(_producerToken, std::make_unique<DependencyOnlineEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, toStartService));
                            handleEventCompletion(startServiceEvt);
                        }
                        break;
                    } else {
                        handleEventCompletion(startServiceEvt);
                    }
                }
                    break;
                case DoWorkEvent::TYPE: {
                    SPDLOG_DEBUG("DoWorkEvent");
                    auto doWorkevt = static_cast<DoWorkEvent *>(evt.get());

                    handleEventCompletion(doWorkevt);
                }
                    break;
                case RemoveCallbacksEvent::TYPE: {
                    SPDLOG_DEBUG("RemoveCallbacksEvent");
                    auto removeCallbacksEvt = static_cast<RemoveCallbacksEvent *>(evt.get());

                    _completionCallbacks.erase(removeCallbacksEvt->key);
                    _errorCallbacks.erase(removeCallbacksEvt->key);
                }
                    break;
                case RemoveTrackerEvent::TYPE: {
                    SPDLOG_DEBUG("RemoveCallbacksEvent");
                    auto removeTrackerEvt = static_cast<RemoveTrackerEvent *>(evt.get());

                    _dependencyRequestTrackers.erase(removeTrackerEvt->interfaceNameHash);
                    _dependencyUndoRequestTrackers.erase(removeTrackerEvt->interfaceNameHash);
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

Cppelix::EventHandlerRegistration::~EventHandlerRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushEvent<RemoveCallbacksEvent>(0, _key);
    }
}

Cppelix::DependencyTrackerRegistration::~DependencyTrackerRegistration() {
    if(_mgr != nullptr) {
        _mgr->pushEvent<RemoveTrackerEvent>(0, _interfaceNameHash);
    }
}
