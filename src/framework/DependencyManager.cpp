#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO

#include "framework/DependencyManager.h"

std::atomic<bool> quit{false};

void on_sigint([[maybe_unused]] int sig) {
    quit.store(true, std::memory_order_release);
}

void Cppelix::DependencyManager::start() {
    assert(_logger != nullptr);
    LOG_DEBUG(_logger, "starting dm");

    ::signal(SIGINT, on_sigint);

    for(auto &[key, lifecycleManager] : _services) {
        if(quit.load(std::memory_order_acquire)) {
            break;
        }

        if(!lifecycleManager->shouldStart()) {
            LOG_DEBUG(_logger, "lifecycleManager {} not ready to start yet", lifecycleManager->name());
            continue;
        }

        LOG_DEBUG(_logger, "starting lifecycleManager {}", lifecycleManager->name());

        if (!lifecycleManager->start()) {
            LOG_TRACE(_logger, "Couldn't start ServiceManager {}", lifecycleManager->name());
            continue;
        }

        _eventQueue.enqueue(_producerToken, std::make_unique<DependencyOnlineEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, lifecycleManager));
    }

    while(!quit.load(std::memory_order_acquire)) {
        std::unique_ptr<Event> evt{nullptr};
        while (_eventQueue.try_dequeue(_consumerToken, evt)) {
            switch(evt->type) {
                case DependencyOnlineEvent::TYPE: {
                    SPDLOG_LOGGER_DEBUG(_logger, "DependencyOnlineEvent");
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

                    for(DependencyTracker &tracker : _dependencyTrackers) {
                        if(tracker.type == depOnlineEvt->manager->type()) {
                            tracker.onlineFunc(depOnlineEvt->manager->getServicePointer());
                        }
                    }
                }
                    break;
                case DependencyOfflineEvent::TYPE: {
                    SPDLOG_LOGGER_DEBUG(_logger, "DependencyOfflineEvent");
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

                    for(DependencyTracker &tracker : _dependencyTrackers) {
                        if(tracker.type == depOfflineEvt->manager->type()) {
                            tracker.offlineFunc(depOfflineEvt->manager->getServicePointer());
                        }
                    }
                }
                    break;
                case DependencyRequestEvent::TYPE: {
                    auto depReqEvt = static_cast<DependencyRequestEvent *>(evt.get());
//                            for (auto &possibleTrackingManager : _trackingServices) {
//
//                            }
                }
                    break;
                case QuitEvent::TYPE: {
                    SPDLOG_LOGGER_DEBUG(_logger, "QuitEvent");
                    auto quitEvt = static_cast<QuitEvent *>(evt.get());
                    if(!quitEvt->dependenciesStopped) {
                        for(auto &[key, possibleManager] : _services) {
                            _eventQueue.enqueue(_producerToken, std::make_unique<StopServiceEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), quitEvt->originatingService, possibleManager->serviceId()));
                        }

                        _eventQueue.enqueue(_producerToken, std::make_unique<QuitEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), quitEvt->originatingService, true));
                    } else {
                        quit.store(true, std::memory_order_release);
                    }
                }
                    break;
                case StopServiceEvent::TYPE: {
                    SPDLOG_LOGGER_DEBUG(_logger, "StopServiceEvent");
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
                case StartServiceEvent::TYPE: {
                    SPDLOG_LOGGER_DEBUG(_logger, "StartServiceEvent");
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
            }
        }

        std::this_thread::sleep_for(1ms);
    }

    for(auto &[key, manager] : _services) {
        manager->stop();
    }
}
