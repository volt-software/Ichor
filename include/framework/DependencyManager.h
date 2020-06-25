#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <cassert>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <spdlog/spdlog.h>
#include <concurrentqueue.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "FrameworkListener.h"
#include "ServiceListener.h"
#include "ServiceListener.h"
#include "Service.h"
#include "ComponentLifecycleManager.h"
#include "Events.h"

using namespace std::chrono_literals;

namespace Cppelix {

    std::atomic<bool> quit{false};

    void on_sigint([[maybe_unused]] int sig) {
        quit.store(true, std::memory_order_release);
    }

    struct DependencyTracker {
        DependencyTracker(std::string_view _name, std::function<void(void*)> on, std::function<void(void*)> off) : name(_name), onlineFunc(std::move(on)), offlineFunc(std::move(off)) {}

        std::string_view name;
        std::function<void(void*)> onlineFunc;
        std::function<void(void*)> offlineFunc;
    };

    class DependencyManager {
    public:
        DependencyManager() : _components(), _dependencyTrackers(), _logger(nullptr), _eventQueue{}, _producerToken{_eventQueue}, _consumerToken{_eventQueue} {}

        template<class Interface, class Impl, typename... Required, typename... Optional>
        requires Derived<Impl, Service> && Derived<Impl, Interface>
        [[nodiscard]]
        auto createDependencyComponentManager(RequiredList_t<Required...>, OptionalList_t<Optional...>, CppelixProperties properties = CppelixProperties{}) {
            auto cmpMgr = DependencyComponentLifecycleManager<Interface, Impl>::template create(_logger, "", std::move(properties), RequiredList<Required...>, OptionalList<Optional...>);

            if(_logger != nullptr) {
                LOG_DEBUG(_logger, "added ComponentManager<{}, {}>", typeName<Interface>(), typeName<Impl>());
            }

            cmpMgr->getComponent().injectDependencyManager(this);

            (_eventQueue.enqueue(_producerToken, std::make_unique<DependencyRequestEvent>(cmpMgr, typeName<Optional>(), Dependency{typeName<Interface>(), Interface::version, false})), ...);
            (_eventQueue.enqueue(_producerToken, std::make_unique<DependencyRequestEvent>(cmpMgr, typeName<Required>(), Dependency{typeName<Interface>(), Interface::version, true})), ...);

            _components.emplace_back(cmpMgr);
            return cmpMgr;
        }

        template<class Interface, class Impl>
        requires Derived<Impl, Service> && Derived<Impl, Interface>
        [[nodiscard]]
        std::shared_ptr<ComponentLifecycleManager<Interface, Impl>> createComponentManager(CppelixProperties properties = {}) {
            auto cmpMgr = ComponentLifecycleManager<Interface, Impl>::create(_logger, "", std::move(properties));

            if constexpr (std::is_same<Interface, IFrameworkLogger>::value) {
                _logger = &cmpMgr->getComponent();
            }

            if(_logger != nullptr) {
                LOG_DEBUG(_logger, "added ComponentManager<{}, {}>", typeName<Interface>(), typeName<Impl>());
            }

            cmpMgr->getComponent().injectDependencyManager(this);

            _components.emplace_back(cmpMgr);
            return cmpMgr;
        }

        void start() {
            assert(_logger != nullptr);
            LOG_DEBUG(_logger, "starting dm");

            ::signal(SIGINT, on_sigint);

            for(auto &lifecycleManager : _components) {
                if(quit.load(std::memory_order_acquire)) {
                    break;
                }

                if(!lifecycleManager->shouldStart()) {
                    LOG_DEBUG(_logger, "lifecycleManager {} not ready to start yet", lifecycleManager->name());
                    continue;
                }

                LOG_DEBUG(_logger, "starting lifecycleManager {}", lifecycleManager->name());

                if (!lifecycleManager->start()) {
                    LOG_TRACE(_logger, "Couldn't start ComponentManager {}", lifecycleManager->name());
                    continue;
                }

                _eventQueue.enqueue(_producerToken, std::make_unique<DependencyOnlineEvent>(lifecycleManager));
            }

            while(!quit.load(std::memory_order_acquire)) {
                std::unique_ptr<Event> evt{nullptr};
                while (_eventQueue.try_dequeue(_consumerToken, evt)) {
                    switch(evt->_type) {
                        case DependencyOnlineEvent::type: {
                            auto depOnlineEvt = dynamic_cast<DependencyOnlineEvent *>(evt.get());
                            for (auto &possibleDependentLifecycleManager : _components) {
                                possibleDependentLifecycleManager->dependencyOnline(depOnlineEvt->manager);

                                if (possibleDependentLifecycleManager->shouldStart()) {
                                    if (possibleDependentLifecycleManager->start()) {
                                        LOG_DEBUG(_logger, "Started {}", possibleDependentLifecycleManager->name());
                                        _eventQueue.enqueue(_producerToken, std::make_unique<DependencyOnlineEvent>(possibleDependentLifecycleManager));
                                    } else {
                                        LOG_DEBUG(_logger, "Couldn't start {}", possibleDependentLifecycleManager->name());
                                    }
                                }
                            }

                            for(DependencyTracker &tracker : _dependencyTrackers) {
                                if(tracker.name == depOnlineEvt->manager->type()) {
                                    tracker.onlineFunc(depOnlineEvt->manager->getComponentPointer());
                                }
                            }
                        }
                            break;
                        case DependencyOfflineEvent::type: {
                            auto depOfflineEvt = dynamic_cast<DependencyOfflineEvent *>(evt.get());
                            for (auto &possibleDependentLifecycleManager : _components) {
                                possibleDependentLifecycleManager->dependencyOffline(depOfflineEvt->manager);

                                if (possibleDependentLifecycleManager->shouldStop()) {
                                    if (possibleDependentLifecycleManager->stop()) {
                                        LOG_DEBUG(_logger, "stopped {}", possibleDependentLifecycleManager->name());
                                        _eventQueue.enqueue(_producerToken, std::make_unique<DependencyOfflineEvent>(possibleDependentLifecycleManager));
                                    } else {
                                        LOG_DEBUG(_logger, "Couldn't stop {}", possibleDependentLifecycleManager->name());
                                    }
                                }
                            }

                            for(DependencyTracker &tracker : _dependencyTrackers) {
                                if(tracker.name == depOfflineEvt->manager->type()) {
                                    tracker.offlineFunc(depOfflineEvt->manager->getComponentPointer());
                                }
                            }
                        }
                            break;
                        case DependencyRequestEvent::type: {
                            auto depReqEvt = dynamic_cast<DependencyRequestEvent *>(evt.get());
//                            for (auto &possibleTrackingManager : _trackingComponents) {
//
//                            }
                        }
                            break;
                        case QuitEvent::type: {
                            auto quitEvt = dynamic_cast<QuitEvent *>(evt.get());
                            if(!quitEvt->dependenciesStopped) {
                                for(auto &possibleManager : _components) {
                                    _eventQueue.enqueue(_producerToken, std::make_unique<StopServiceEvent>(possibleManager->serviceId()));
                                }

                                _eventQueue.enqueue(_producerToken, std::make_unique<QuitEvent>(true));
                            } else {
                                quit.store(true, std::memory_order_release);
                            }
                        }
                            break;
                        case StopServiceEvent::type: {
                            auto stopServiceEvt = dynamic_cast<StopServiceEvent *>(evt.get());

                            if(stopServiceEvt->dependenciesStopped) {
                                for(auto &possibleManager : _components) {
                                    if(possibleManager->serviceId() == stopServiceEvt->serviceId) {
                                        if(!possibleManager->stop()) {
                                            LOG_ERROR(_logger, "Couldn't stop component {}: {} but all dependencies stopped", stopServiceEvt->serviceId, possibleManager->name());
                                        }
                                        break;
                                    }
                                }
                            } else {
                                for(auto &possibleManager : _components) {
                                    if(possibleManager->serviceId() == stopServiceEvt->serviceId) {
                                        _eventQueue.enqueue(_producerToken, std::make_unique<DependencyOfflineEvent>(possibleManager));
                                        _eventQueue.enqueue(_producerToken, std::make_unique<StopServiceEvent>(stopServiceEvt->serviceId, true));
                                        break;
                                    }
                                }
                            }
                        }
                            break;
                        case StartServiceEvent::type: {
                            auto startServiceEvt = dynamic_cast<StartServiceEvent *>(evt.get());


                            for(auto &possibleManager : _components) {
                                if(possibleManager->serviceId() == startServiceEvt->serviceId) {
                                    if(possibleManager->shouldStart()) {
                                        possibleManager->start();
                                        _eventQueue.enqueue(_producerToken, std::make_unique<DependencyOnlineEvent>(possibleManager));
                                        break;
                                    }
                                }
                            }
                        }
                            break;
                    }
                }

                std::this_thread::sleep_for(1ms);
            }

            for(auto &manager : _components) {
                manager->stop();
            }
        }

        template <typename T, typename... Args>
        void PushEvent(Args&&... args){
            _eventQueue.enqueue(std::make_unique<T, Args...>(std::forward<Args>(args)...));
        }

        template <class Impl, class Dependency>
        void trackDependencyRequests(Impl *impl) {
            for(auto &possibleImplManager : _components) {
                if(possibleImplManager->type() == typeName<Dependency>()) {
                    //impl->injectDependency(possibleImplManager->);
                }
            }
        }

        template <class Dependency>
        void trackOnlineDependencies(std::function<void(void*)> on, std::function<void(void*)> off) {
            _dependencyTrackers.emplace_back(typeName<Dependency>, std::move(on), std::move(off));
        }

    private:
        std::vector<std::shared_ptr<LifecycleManager>> _components;
        std::vector<DependencyTracker> _dependencyTrackers;
        IFrameworkLogger *_logger;
        moodycamel::ConcurrentQueue<std::unique_ptr<Event>> _eventQueue;
        moodycamel::ProducerToken _producerToken;
        moodycamel::ConsumerToken _consumerToken;
    };
}