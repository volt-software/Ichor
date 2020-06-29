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
#include "Service.h"
#include "ServiceLifecycleManager.h"
#include "Events.h"
#include "framework/Callback.h"

using namespace std::chrono_literals;

namespace Cppelix {

    class DependencyManager;

    class [[nodiscard]] EventHandlerRegistration {
    public:
        EventHandlerRegistration(DependencyManager *mgr, CallbackKey key) noexcept : _mgr(mgr), _key(key) {}
        EventHandlerRegistration() noexcept = default;
        ~EventHandlerRegistration();

        EventHandlerRegistration(const EventHandlerRegistration&) = delete;
        EventHandlerRegistration(EventHandlerRegistration&&) noexcept = default;
        EventHandlerRegistration& operator=(const EventHandlerRegistration&) = delete;
        EventHandlerRegistration& operator=(EventHandlerRegistration&&) noexcept = default;
    private:
        DependencyManager *_mgr{nullptr};
        CallbackKey _key{0, 0};
    };

    class [[nodiscard]] DependencyTrackerRegistration {
    public:
        DependencyTrackerRegistration(DependencyManager *mgr, uint64_t interfaceNameHash) noexcept : _mgr(mgr), _interfaceNameHash(interfaceNameHash) {}
        DependencyTrackerRegistration() noexcept = default;
        ~DependencyTrackerRegistration();

        DependencyTrackerRegistration(const DependencyTrackerRegistration&) = delete;
        DependencyTrackerRegistration(DependencyTrackerRegistration&&) noexcept = default;
        DependencyTrackerRegistration& operator=(const DependencyTrackerRegistration&) = delete;
        DependencyTrackerRegistration& operator=(DependencyTrackerRegistration&&) noexcept = default;
    private:
        DependencyManager *_mgr{nullptr};
        uint64_t _interfaceNameHash{0};
    };

    struct DependencyTrackerInfo {
        DependencyTrackerInfo() = default;
        DependencyTrackerInfo(uint64_t _trackingServiceId, std::function<void(Event const * const)> _trackFunc) noexcept : trackingServiceId(_trackingServiceId), trackFunc(std::move(_trackFunc)) {}
        ~DependencyTrackerInfo() = default;
        uint64_t trackingServiceId;
        std::function<void(Event const * const)> trackFunc;
    };

    class DependencyManager {
    public:
        DependencyManager() : _services(), _dependencyRequestTrackers(), _dependencyUndoRequestTrackers(), _completionCallbacks{}, _errorCallbacks{}, _logger(nullptr), _eventQueue{}, _producerToken{_eventQueue}, _consumerToken{_eventQueue}, _eventIdCounter{0} {}

        template<class Interface, class Impl, typename... Required, typename... Optional>
        requires Derived<Impl, Service> && Derived<Impl, Interface>
        [[nodiscard]]
        auto createDependencyServiceManager(RequiredList_t<Required...>, OptionalList_t<Optional...>, CppelixProperties properties = CppelixProperties{}) {
            auto cmpMgr = DependencyServiceLifecycleManager<Interface, Impl>::template create(_logger, "", properties, RequiredList<Required...>, OptionalList<Optional...>);

            if(_logger != nullptr) {
                LOG_DEBUG(_logger, "added ServiceManager<{}, {}>", typeName<Interface>(), typeName<Impl>());
            }

            cmpMgr->getService().injectDependencyManager(this);

            for(auto &[key, mgr] : _services) {
                if(mgr->getServiceState() == ServiceState::ACTIVE) {
                    cmpMgr->dependencyOnline(mgr);
                }
            }

            (_eventQueue.enqueue(_producerToken, std::make_unique<DependencyRequestEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), cmpMgr->serviceId(), cmpMgr, Dependency{typeNameHash<Optional>(), Optional::version, false}, properties)), ...);
            (_eventQueue.enqueue(_producerToken, std::make_unique<DependencyRequestEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), cmpMgr->serviceId(), cmpMgr, Dependency{typeNameHash<Required>(), Required::version, true}, properties)), ...);
            _eventQueue.enqueue(_producerToken, std::make_unique<StartServiceEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, cmpMgr->getService().getServiceId()));

            _services.emplace(cmpMgr->serviceId(), cmpMgr);
            return cmpMgr;
        }

        template<class Interface, class Impl>
        requires Derived<Impl, Service> && Derived<Impl, Interface>
        [[nodiscard]]
        std::shared_ptr<ServiceLifecycleManager<Interface, Impl>> createServiceManager(CppelixProperties properties = {}) {
            auto cmpMgr = ServiceLifecycleManager<Interface, Impl>::create(_logger, "", std::move(properties));

            if constexpr (std::is_same<Interface, IFrameworkLogger>::value) {
                _logger = &cmpMgr->getService();
            }

            if(_logger != nullptr) {
                LOG_DEBUG(_logger, "added ServiceManager<{}, {}>", typeName<Interface>(), typeName<Impl>());
            }

            cmpMgr->getService().injectDependencyManager(this);

            _eventQueue.enqueue(_producerToken, std::make_unique<StartServiceEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, cmpMgr->getService().getServiceId()));

            _services.emplace(cmpMgr->serviceId(), cmpMgr);
            return cmpMgr;
        }

        template <typename EventT, typename... Args>
        requires Derived<EventT, Event>
        uint64_t pushEvent(uint64_t originatingServiceId, Args&&... args){
            uint64_t eventId = _eventIdCounter.fetch_add(1, std::memory_order_acq_rel);
            _eventQueue.enqueue(std::make_unique<EventT, uint64_t, uint64_t, Args...>(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), std::forward<Args>(args)...));
            return eventId;
        }

        template <typename Interface, typename Impl>
        requires Derived<Impl, Service> && ImplementsTrackingHandlers<Impl, Interface>
        [[nodiscard]]
        std::unique_ptr<DependencyTrackerRegistration> registerDependencyTracker(uint64_t serviceId, Impl *impl) {
            auto requestTrackersForType = _dependencyRequestTrackers.find(typeNameHash<Interface>());
            auto undoRequestTrackersForType = _dependencyUndoRequestTrackers.find(typeNameHash<Interface>());

            DependencyTrackerInfo requestInfo{impl->getServiceId(), [impl](Event const * const evt){ impl->handleDependencyRequest(static_cast<Interface*>(nullptr), static_cast<DependencyRequestEvent const *>(evt)); }};
            DependencyTrackerInfo undoRequestInfo{impl->getServiceId(), [impl](Event const * const evt){ impl->handleDependencyUndoRequest(static_cast<Interface*>(nullptr), static_cast<DependencyUndoRequestEvent const *>(evt)); }};

            if(requestTrackersForType == end(_dependencyRequestTrackers)) {
                _dependencyRequestTrackers.emplace(typeNameHash<Interface>(), std::vector<DependencyTrackerInfo>{std::move(requestInfo)});
            } else {
                requestTrackersForType->second.emplace_back(std::move(requestInfo));
            }

            if(undoRequestTrackersForType == end(_dependencyUndoRequestTrackers)) {
                _dependencyUndoRequestTrackers.emplace(typeNameHash<Interface>(), std::vector<DependencyTrackerInfo>{undoRequestInfo});
            } else {
                undoRequestTrackersForType->second.emplace_back(std::move(undoRequestInfo));
            }

            for(auto &[key, mgr] : _services) {
                auto const * depInfo = mgr->getDependencyInfo();

                if(depInfo == nullptr) {
                    continue;
                }

                for (const auto &dependency : depInfo->_dependencies) {
                    if(dependency.interfaceNameHash == typeNameHash<Interface>()) {
                        DependencyRequestEvent evt{0, mgr->serviceId(), mgr, Dependency{dependency.interfaceNameHash, dependency.interfaceVersion, dependency.required}, mgr->getProperties()};
                        requestInfo.trackFunc(&evt);
                    }
                }
            }

            // I think there's a bug in GCC 10.1, where if I don't make this a unique_ptr, the EventHandlerRegistration destructor immediately gets called for some reason.
            // Even if the result is stored in a variable at the caller site.
            return std::make_unique<DependencyTrackerRegistration>(this, typeNameHash<Interface>());
        }

        template <typename EventT, typename Impl>
        requires Derived<EventT, Event> && ImplementsEventHandlers<Impl, EventT>
        [[nodiscard]]
        std::unique_ptr<EventHandlerRegistration> registerEventCallbacks(uint64_t serviceId, Impl *impl) {
            CallbackKey key{serviceId, EventT::TYPE};
            _completionCallbacks.emplace(key, [impl](Event const * const evt){ impl->handleCompletion(static_cast<EventT const * const>(evt)); });
            _errorCallbacks.emplace(key, [impl](Event const * const evt){ impl->handleError(static_cast<EventT const * const>(evt)); });
            // I think there's a bug in GCC 10.1, where if I don't make this a unique_ptr, the EventHandlerRegistration destructor immediately gets called for some reason.
            // Even if the result is stored in a variable at the caller site.
            return std::make_unique<EventHandlerRegistration>(this, key);
        }

        void start();

    private:
        template <typename EventT>
        requires Derived<EventT, Event>
        void handleEventError(EventT *evt) {
            if(evt->originatingService == 0) {
                return;
            }

            auto service = _services.find(evt->originatingService);
            if(service == end(_services) || service->second->getServiceState() != ServiceState::ACTIVE) {
                return;
            }

            auto callback = _errorCallbacks.find(CallbackKey{evt->originatingService, EventT::TYPE});
            if(callback == end(_errorCallbacks)) {
                return;
            }

            callback->second(evt);
        }


        template <typename EventT>
        requires Derived<EventT, Event>
        void handleEventCompletion(EventT *evt) {
            if(evt->originatingService == 0) {
                return;
            }

            auto service = _services.find(evt->originatingService);
            if(service == end(_services) || service->second->getServiceState() != ServiceState::ACTIVE) {
                return;
            }

            auto callback = _completionCallbacks.find(CallbackKey{evt->originatingService, EventT::TYPE});
            if(callback == end(_completionCallbacks)) {
                return;
            }

            callback->second(evt);
        }

        std::unordered_map<uint64_t, std::shared_ptr<LifecycleManager>> _services;
        std::unordered_map<uint64_t, std::vector<DependencyTrackerInfo>> _dependencyRequestTrackers;
        std::unordered_map<uint64_t, std::vector<DependencyTrackerInfo>> _dependencyUndoRequestTrackers;
        std::unordered_map<CallbackKey, std::function<void(Event const * const)>> _completionCallbacks;
        std::unordered_map<CallbackKey, std::function<void(Event const * const)>> _errorCallbacks;
        IFrameworkLogger *_logger;
        moodycamel::ConcurrentQueue<std::unique_ptr<Event>> _eventQueue;
        moodycamel::ProducerToken _producerToken;
        moodycamel::ConsumerToken _consumerToken;
        std::atomic<uint64_t> _eventIdCounter;

        friend class EventHandlerRegistration;
    };
}