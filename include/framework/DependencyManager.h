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
#include "ServiceLifecycleManager.h"
#include "Events.h"
#include "framework/Callback.h"

using namespace std::chrono_literals;

namespace Cppelix {

    struct DependencyTracker {
        DependencyTracker(uint64_t _type, std::function<void(void*)> on, std::function<void(void*)> off) : type(_type), onlineFunc(std::move(on)), offlineFunc(std::move(off)) {}

        uint64_t type;
        std::function<void(void*)> onlineFunc;
        std::function<void(void*)> offlineFunc;
    };

    class DependencyManager {
    public:
        DependencyManager() : _services(), _dependencyTrackers(), _completionCallbacks{}, _errorCallbacks{}, _logger(nullptr), _eventQueue{}, _producerToken{_eventQueue}, _consumerToken{_eventQueue}, _eventIdCounter{0} {}

        template<class Interface, class Impl, typename... Required, typename... Optional>
        requires Derived<Impl, Service> && Derived<Impl, Interface>
        [[nodiscard]]
        auto createDependencyServiceManager(RequiredList_t<Required...>, OptionalList_t<Optional...>, CppelixProperties properties = CppelixProperties{}) {
            auto cmpMgr = DependencyServiceLifecycleManager<Interface, Impl>::template create(_logger, "", std::move(properties), RequiredList<Required...>, OptionalList<Optional...>);

            if(_logger != nullptr) {
                LOG_DEBUG(_logger, "added ServiceManager<{}, {}>", typeName<Interface>(), typeName<Impl>());
            }

            cmpMgr->getService().injectDependencyManager(this);

            (_eventQueue.enqueue(_producerToken, std::make_unique<DependencyRequestEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, cmpMgr, typeNameHash<Optional>(), Dependency{typeNameHash<Interface>(), Interface::version, false})), ...);
            (_eventQueue.enqueue(_producerToken, std::make_unique<DependencyRequestEvent>(_eventIdCounter.fetch_add(1, std::memory_order_acq_rel), 0, cmpMgr, typeNameHash<Required>(), Dependency{typeNameHash<Interface>(), Interface::version, true})), ...);

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

        template <class Impl, class Dependency>
        void trackDependencyRequests(Impl *impl) {
            for(auto &[key, possibleImplManager] : _services) {
                if(possibleImplManager->type() == typeNameHash<Dependency>()) {
                    //impl->injectDependency(possibleImplManager->);
                }
            }
        }

        template <class Dependency>
        void trackOnlineDependencies(std::function<void(void*)> on, std::function<void(void*)> off) {
            _dependencyTrackers.emplace_back(typeNameHash<Dependency>(), std::move(on), std::move(off));
        }

        template <typename EventT, typename Impl>
        requires Derived<EventT, Event> && ImplementsCompletionHandler<Impl, EventT>
        void registerCompletionCallback(uint64_t serviceId, Impl *impl) {
            _completionCallbacks.emplace(CallbackKey{serviceId, EventT::TYPE}, [impl](Event const * const evt){ impl->handleCompletion(static_cast<EventT const * const>(evt)); });
        }

        template <typename EventT, typename Impl>
        requires Derived<EventT, Event> && ImplementsErrorHandler<Impl, EventT>
        void registerErrorCallback(uint64_t serviceId, Impl *impl) {
            _errorCallbacks.emplace(CallbackKey{serviceId, EventT::TYPE}, [impl](Event const * const evt){ impl->handleError(static_cast<EventT const * const>(evt)); });
        }

        void start();

    private:
        template <typename EventT>
        requires Derived<EventT, Event>
        void handleEventError(EventT *evt) {
            if(evt->originatingService == 0) {
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

            auto callback = _completionCallbacks.find(CallbackKey{evt->originatingService, EventT::TYPE});
            if(callback == end(_completionCallbacks)) {
                return;
            }

            callback->second(evt);
        }

        std::unordered_map<uint64_t, std::shared_ptr<LifecycleManager>> _services;
        std::vector<DependencyTracker> _dependencyTrackers;
        std::unordered_map<CallbackKey, std::function<void(Event const * const)>> _completionCallbacks;
        std::unordered_map<CallbackKey, std::function<void(Event const * const)>> _errorCallbacks;
        IFrameworkLogger *_logger;
        moodycamel::ConcurrentQueue<std::unique_ptr<Event>> _eventQueue;
        moodycamel::ProducerToken _producerToken;
        moodycamel::ConsumerToken _consumerToken;
        std::atomic<uint64_t> _eventIdCounter;
    };
}