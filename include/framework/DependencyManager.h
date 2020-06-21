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
#include "BundleListener.h"
#include "ServiceListener.h"
#include "Bundle.h"
#include "ComponentLifecycleManager.h"

using namespace std::chrono_literals;

namespace Cppelix {

    std::atomic<bool> quit{false};

    void on_sigint([[maybe_unused]] int sig) {
        quit.store(true, std::memory_order_release);
    }

    struct Event {
        Event(uint64_t type) noexcept : _type{type} {}
        virtual ~Event() = default;
        uint64_t _type;
    };

    struct DependencyOnlineEvent : public Event {
        explicit DependencyOnlineEvent(const std::shared_ptr<LifecycleManager> _manager) noexcept : Event(type), manager(std::move(_manager)) {}

        const std::shared_ptr<LifecycleManager> manager;
        static constexpr uint64_t type = 1;
    };

    struct DependencyOfflineEvent : public Event {
        explicit DependencyOfflineEvent(const std::shared_ptr<LifecycleManager> _manager) noexcept : Event(type), manager(std::move(_manager)) {}

        const std::shared_ptr<LifecycleManager> manager;
        static constexpr uint64_t type = 2;
    };

    struct DependencyRequestEvent : public Event {
        explicit DependencyRequestEvent(const std::shared_ptr<LifecycleManager> _manager, const std::string_view _requestedType, Dependency _dependency) noexcept : Event(type), manager(std::move(_manager)), requestedType(_requestedType), dependency(std::move(_dependency)) {}

        const std::shared_ptr<LifecycleManager> manager;
        const std::string_view requestedType;
        const Dependency dependency;
        static constexpr uint64_t type = 3;
    };

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
        requires Derived<Impl, Bundle> && Derived<Impl, Interface>
        [[nodiscard]]
        auto createDependencyComponentManager(RequiredList_t<Required...>, OptionalList_t<Optional...>) {
            auto cmpMgr = DependencyComponentLifecycleManager<Interface, Impl>::template create(_logger, "", RequiredList<Required...>, OptionalList<Optional...>);

            if(_logger != nullptr) {
                LOG_DEBUG(_logger, "added ComponentManager<{}, {}>", typeName<Interface>(), typeName<Impl>());
            }

            cmpMgr->getComponent().injectDependencyManager(this);

            (_eventQueue.enqueue(_producerToken, std::make_unique<DependencyRequestEvent>(cmpMgr, typeName<Optional>(), Dependency{typeName<Interface>(), Interface::version, false, std::unordered_map<std::string, std::unique_ptr<IProperty>>{}})), ...);
            (_eventQueue.enqueue(_producerToken, std::make_unique<DependencyRequestEvent>(cmpMgr, typeName<Required>(), Dependency{typeName<Interface>(), Interface::version, true, std::unordered_map<std::string, std::unique_ptr<IProperty>>{}})), ...);

            _components.emplace_back(cmpMgr);
            return cmpMgr;
        }

        template<class Interface, class Impl>
        requires Derived<Impl, Bundle> && Derived<Impl, Interface>
        [[nodiscard]]
        std::shared_ptr<ComponentLifecycleManager<Interface, Impl>> createComponentManager() {
            auto cmpMgr = ComponentLifecycleManager<Interface, Impl>::create(_logger, "");

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
                    }
                }

                std::this_thread::sleep_for(1ms);
            }

            for(auto &manager : _components) {
                manager->stop();
            }
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