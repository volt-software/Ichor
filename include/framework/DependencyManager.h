#pragma once

#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <cassert>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <condition_variable>
#include <cppcoro/generator.hpp>
#include <concurrentqueue.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "Service.h"
#include "LifecycleManager.h"
#include "Events.h"
#include "framework/Callback.h"
#include "Filter.h"

using namespace std::chrono_literals;

namespace Cppelix {

    class DependencyManager;
    class CommunicationChannel;

    class [[nodiscard]] EventStackUniquePtr final {
    public:
        EventStackUniquePtr() = default;

        template <typename T, typename... Args>
        requires Derived<T, Event>
        static EventStackUniquePtr create(Args&&... args) {
            static_assert(sizeof(T) < 128, "size not big enough to hold T");
            EventStackUniquePtr ptr;
            new (ptr._buffer.data()) T(std::forward<Args>(args)...);
            ptr._type = T::TYPE;
            return ptr;
        }

        EventStackUniquePtr(const EventStackUniquePtr&) = delete;
        EventStackUniquePtr(EventStackUniquePtr&& other) noexcept {
            if(!_empty) {
                reinterpret_cast<Event *>(_buffer.data())->~Event();
            }
            _buffer = other._buffer;
            other._empty = true;
            _empty = false;
            _type = other._type;
        }


        EventStackUniquePtr& operator=(const EventStackUniquePtr&) = delete;
        EventStackUniquePtr& operator=(EventStackUniquePtr &&other) noexcept {
            if(!_empty) {
                reinterpret_cast<Event *>(_buffer.data())->~Event();
            }
            _buffer = other._buffer;
            other._empty = true;
            _empty = false;
            _type = other._type;
            return *this;
        }

        ~EventStackUniquePtr() {
            if(!_empty) {
                reinterpret_cast<Event *>(_buffer.data())->~Event();
            }
        }

        template <typename T>
        requires Derived<T, Event>
        [[nodiscard]] T* getT() {
            if(_empty) {
                throw std::runtime_error("empty");
            }

            return reinterpret_cast<T*>(_buffer.data());
        }

        [[nodiscard]] Event* get() {
            if(_empty) {
                throw std::runtime_error("empty");
            }

            return reinterpret_cast<Event*>(_buffer.data());
        }

        [[nodiscard]] uint64_t getType() const noexcept {
            return _type;
        }
    private:

        std::array<uint8_t, 128> _buffer;
        uint64_t _type{0};
        bool _empty{true};
    };

    class [[nodiscard]] EventCompletionHandlerRegistration final {
    public:
        EventCompletionHandlerRegistration(DependencyManager *mgr, CallbackKey key) noexcept : _mgr(mgr), _key(key) {}
        EventCompletionHandlerRegistration() noexcept = default;
        ~EventCompletionHandlerRegistration();

        EventCompletionHandlerRegistration(const EventCompletionHandlerRegistration&) = delete;
        EventCompletionHandlerRegistration(EventCompletionHandlerRegistration&&) noexcept = default;
        EventCompletionHandlerRegistration& operator=(const EventCompletionHandlerRegistration&) = delete;
        EventCompletionHandlerRegistration& operator=(EventCompletionHandlerRegistration&&) noexcept = default;
    private:
        DependencyManager *_mgr{nullptr};
        CallbackKey _key{0, 0};
    };

    class [[nodiscard]] EventHandlerRegistration final {
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



    class [[nodiscard]] DependencyTrackerRegistration final {
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

    struct DependencyTrackerInfo final {
        DependencyTrackerInfo(uint64_t _trackingServiceId, std::function<void(Event const * const)> _trackFunc) noexcept : trackingServiceId(_trackingServiceId), trackFunc(std::move(_trackFunc)) {}
        ~DependencyTrackerInfo() = default;
        uint64_t trackingServiceId;
        std::function<void(Event const * const)> trackFunc;
    };

    class DependencyManager final {
    public:
        DependencyManager() : _services(), _dependencyRequestTrackers(), _dependencyUndoRequestTrackers(), _completionCallbacks{}, _errorCallbacks{}, _logger(nullptr), _eventQueue{}, _eventQueueMutex{}, _eventIdCounter{0}, _quit{false}, _communicationChannel(nullptr), _id(_managerIdCounter++) {}

        template<class Interface, class Impl, typename... Required, typename... Optional>
        requires Derived<Impl, Service> && Derived<Impl, Interface> && std::has_virtual_destructor_v<Interface>
        auto createServiceManager(RequiredList_t<Required...>, OptionalList_t<Optional...>, CppelixProperties properties = CppelixProperties{}) {
            if constexpr(sizeof...(Required) == 0 && sizeof...(Optional) == 0) {
                return createServiceManager<Interface, Impl>(std::move(properties));
            }

            auto cmpMgr = LifecycleManager<Interface, Impl, Required..., Optional...>::template create(_logger, "", std::move(properties), RequiredList<Required...>, OptionalList<Optional...>);

            if(_logger != nullptr) {
                LOG_DEBUG(_logger, "added ServiceManager<{}, {}>", typeName<Interface>(), typeName<Impl>());
            }

            cmpMgr->getService().injectDependencyManager(this);

            for(auto &[key, mgr] : _services) {
                if(mgr->getServiceState() == ServiceState::ACTIVE) {
                    auto filterProp = mgr->getProperties()->find("Filter");
                    const Filter *filter = nullptr;
                    if(filterProp != end(*mgr->getProperties())) {
                        filter = std::any_cast<const Filter>(&filterProp->second);
                    }

                    if(filter != nullptr && !filter->compareTo(cmpMgr)) {
                        continue;
                    }

                    cmpMgr->dependencyOnline(mgr);
                }
            }

            (pushEventInternal<DependencyRequestEvent>(cmpMgr->serviceId(), INTERNAL_EVENT_PRIORITY, cmpMgr, Dependency{typeNameHash<Optional>(), Optional::version, false}, cmpMgr->getProperties()), ...);
            (pushEventInternal<DependencyRequestEvent>(cmpMgr->serviceId(), INTERNAL_EVENT_PRIORITY, cmpMgr, Dependency{typeNameHash<Required>(), Required::version, true}, cmpMgr->getProperties()), ...);
            pushEventInternal<StartServiceEvent>(0, INTERNAL_EVENT_PRIORITY, cmpMgr->serviceId());

            _services.emplace(cmpMgr->serviceId(), cmpMgr);
            return &cmpMgr->getService();
        }

        template<class Interface, class Impl>
        requires Derived<Impl, Service> && Derived<Impl, Interface> && std::has_virtual_destructor_v<Interface>
        auto createServiceManager(CppelixProperties properties = {}) {
            auto cmpMgr = LifecycleManager<Interface, Impl>::create(_logger, "", std::move(properties));

            if constexpr (std::is_same<Interface, IFrameworkLogger>::value) {
                _logger = &cmpMgr->getService();
            }

            if(_logger != nullptr) {
                LOG_DEBUG(_logger, "added ServiceManager<{}, {}>", typeName<Interface>(), typeName<Impl>());
            }

            cmpMgr->getService().injectDependencyManager(this);

            pushEventInternal<StartServiceEvent>(0, INTERNAL_EVENT_PRIORITY, cmpMgr->serviceId());

            _services.emplace(cmpMgr->serviceId(), cmpMgr);
            return &cmpMgr->getService();
        }

        /// Push event into event loop with specified priority
        /// \tparam EventT Type of event to push, has to derive from Event
        /// \tparam Args auto-deducible arguments for EventT constructor
        /// \param originatingServiceId service that is pushing the event
        /// \param args arguments for EventT constructor
        /// \return event id (can be used in completion/error handlers)
        template <typename EventT, typename... Args>
        requires Derived<EventT, Event>
        uint64_t pushPrioritisedEvent(uint64_t originatingServiceId, uint64_t priority, Args&&... args){
            static_assert(sizeof(EventT) < 128, "event type cannot be larger than 128 bytes");

            if(_quit.load(std::memory_order_acquire)) {
                return 0;
            }

            uint64_t eventId = _eventIdCounter.fetch_add(1, std::memory_order_acq_rel);
            _eventQueueMutex.lock();
            _eventQueue.emplace(priority, EventStackUniquePtr::create<EventT>(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), std::forward<uint64_t>(priority), std::forward<Args>(args)...));
            _eventQueueMutex.unlock();
            _wakeUp.notify_all();
            return eventId;
        }

        /// Push event into event loop with the default priority
        /// \tparam EventT Type of event to push, has to derive from Event
        /// \tparam Args auto-deducible arguments for EventT constructor
        /// \param originatingServiceId service that is pushing the event
        /// \param args arguments for EventT constructor
        /// \return event id (can be used in completion/error handlers)
        template <typename EventT, typename... Args>
        requires Derived<EventT, Event>
        uint64_t pushEvent(uint64_t originatingServiceId, Args&&... args){
            static_assert(sizeof(EventT) < 128, "event type cannot be larger than 128 bytes");

            if(_quit.load(std::memory_order_acquire)) {
                return 0;
            }

            uint64_t eventId = _eventIdCounter.fetch_add(1, std::memory_order_acq_rel);
            _eventQueueMutex.lock();
            _eventQueue.emplace(INTERNAL_EVENT_PRIORITY, EventStackUniquePtr::create<EventT>(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), INTERNAL_EVENT_PRIORITY, std::forward<Args>(args)...));
            _eventQueueMutex.unlock();
            _wakeUp.notify_all();
            return eventId;
        }

        template <typename Interface, typename Impl>
        requires Derived<Impl, Service> && ImplementsTrackingHandlers<Impl, Interface>
        [[nodiscard]]
        /// Register handlers for when dependencies get requested/unrequested
        /// \tparam Interface type of interface where dependency is requested for (has to derive from Event)
        /// \tparam Impl type of class registering handler (auto-deducible)
        /// \param serviceId id of service registering handler
        /// \param impl class that is registering handler
        /// \return RAII handler, removes registration upon destruction
        std::unique_ptr<DependencyTrackerRegistration> registerDependencyTracker(uint64_t serviceId, Impl *impl) {
            auto requestTrackersForType = _dependencyRequestTrackers.find(typeNameHash<Interface>());
            auto undoRequestTrackersForType = _dependencyUndoRequestTrackers.find(typeNameHash<Interface>());

            DependencyTrackerInfo requestInfo{impl->getServiceId(), [impl](Event const * const evt){ impl->handleDependencyRequest(static_cast<Interface*>(nullptr), static_cast<DependencyRequestEvent const *>(evt)); }};
            DependencyTrackerInfo undoRequestInfo{impl->getServiceId(), [impl](Event const * const evt){ impl->handleDependencyUndoRequest(static_cast<Interface*>(nullptr), static_cast<DependencyUndoRequestEvent const *>(evt)); }};

            for(auto &[key, mgr] : _services) {
                auto const * depInfo = mgr->getDependencyInfo();

                if(depInfo == nullptr) {
                    continue;
                }

                for (const auto &dependency : depInfo->_dependencies) {
                    if(dependency.interfaceNameHash == typeNameHash<Interface>()) {
                        DependencyRequestEvent evt{0, mgr->serviceId(), INTERNAL_EVENT_PRIORITY, mgr, Dependency{dependency.interfaceNameHash, dependency.interfaceVersion, dependency.required}, mgr->getProperties()};
                        requestInfo.trackFunc(&evt);
                    }
                }
            }

            if(requestTrackersForType == end(_dependencyRequestTrackers)) {
                _dependencyRequestTrackers.emplace(typeNameHash<Interface>(), std::vector<DependencyTrackerInfo>{requestInfo});
            } else {
                requestTrackersForType->second.emplace_back(std::move(requestInfo));
            }

            if(undoRequestTrackersForType == end(_dependencyUndoRequestTrackers)) {
                _dependencyUndoRequestTrackers.emplace(typeNameHash<Interface>(), std::vector<DependencyTrackerInfo>{undoRequestInfo});
            } else {
                undoRequestTrackersForType->second.emplace_back(std::move(undoRequestInfo));
            }

            // I think there's a bug in GCC 10.1, where if I don't make this a unique_ptr, the DependencyTrackerRegistration destructor immediately gets called for some reason.
            // Even if the result is stored in a variable at the caller site.
            return std::make_unique<DependencyTrackerRegistration>(this, typeNameHash<Interface>());
        }

        template <typename EventT, typename Impl>
        requires Derived<EventT, Event> && ImplementsEventCompletionHandlers<Impl, EventT>
        [[nodiscard]]
        /// Register event error/completion handlers
        /// \tparam EventT type of event (has to derive from Event)
        /// \tparam Impl type of class registering handler (auto-deducible)
        /// \param serviceId id of service registering handler
        /// \param impl class that is registering handler
        /// \return RAII handler, removes registration upon destruction
        std::unique_ptr<EventCompletionHandlerRegistration> registerEventCompletionCallbacks(uint64_t serviceId, Impl *impl) {
            CallbackKey key{serviceId, EventT::TYPE};
            _completionCallbacks.emplace(key, [impl](Event const * const evt){ impl->handleCompletion(static_cast<EventT const * const>(evt)); });
            _errorCallbacks.emplace(key, [impl](Event const * const evt){ impl->handleError(static_cast<EventT const * const>(evt)); });
            // I think there's a bug in GCC 10.1, where if I don't make this a unique_ptr, the EventCompletionHandlerRegistration destructor immediately gets called for some reason.
            // Even if the result is stored in a variable at the caller site.
            return std::make_unique<EventCompletionHandlerRegistration>(this, key);
        }

        template <typename EventT, typename Impl>
        requires Derived<EventT, Event> && ImplementsEventHandlers<Impl, EventT>
        [[nodiscard]]
        /// Register an event handler
        /// \tparam EventT type of event (has to derive from Event)
        /// \tparam Impl type of class registering handler (auto-deducible)
        /// \param serviceId id of service registering handler
        /// \param impl class that is registering handler
        /// \param targetServiceId optional service id to filter registering for, if empty, receive all events of type EventT
        /// \return RAII handler, removes registration upon destruction
        std::unique_ptr<EventHandlerRegistration> registerEventHandler(uint64_t serviceId, Impl *impl, std::optional<uint64_t> targetServiceId = {}) {
            auto existingHandlers = _eventCallbacks.find(EventT::TYPE);
            if(existingHandlers == end(_eventCallbacks)) {
                _eventCallbacks.emplace(EventT::TYPE, std::vector<EventCallbackInfo>{EventCallbackInfo{serviceId, targetServiceId,
                        [impl](Event const * const evt){ return impl->handleEvent(static_cast<EventT const * const>(evt)); }}
                });
            } else {
                existingHandlers->second.emplace_back(EventCallbackInfo{serviceId, targetServiceId, [impl](Event const * const evt){ return impl->handleEvent(static_cast<EventT const * const>(evt)); }});
            }
            // I think there's a bug in GCC 10.1, where if I don't make this a unique_ptr, the EventHandlerRegistration destructor immediately gets called for some reason.
            // Even if the result is stored in a variable at the caller site.
            return std::make_unique<EventHandlerRegistration>(this, CallbackKey{serviceId, EventT::TYPE});
        }

        [[nodiscard]] uint64_t getId() const {
            return _id;
        }

        ///
        /// \return Potentially nullptr
        [[nodiscard]] CommunicationChannel* getCommunicationChannel() {
            return _communicationChannel;
        }

        void start();

    private:
        template <typename EventT>
        requires Derived<EventT, Event>
        void handleEventError(EventT const * const evt) const {
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

        void handleEventCompletion(Event const * const evt) const;

        void broadcastEvent(Event const * const evt);

        void setCommunicationChannel(CommunicationChannel *channel);

        template <typename EventT, typename... Args>
        requires Derived<EventT, Event>
        uint64_t pushEventInternal(uint64_t originatingServiceId, uint64_t priority, Args&&... args){
            static_assert(sizeof(EventT) < 128, "event type cannot be larger than 128 bytes");

            uint64_t eventId = _eventIdCounter.fetch_add(1, std::memory_order_acq_rel);
            _eventQueueMutex.lock();
            _eventQueue.emplace(priority, EventStackUniquePtr::create<EventT>(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), std::forward<uint64_t>(priority), std::forward<Args>(args)...));
            _eventQueueMutex.unlock();
            _wakeUp.notify_all();
            return eventId;
        }

        std::unordered_map<uint64_t, std::shared_ptr<ILifecycleManager>> _services; // key = service id
        std::unordered_map<uint64_t, std::vector<DependencyTrackerInfo>> _dependencyRequestTrackers; // key = interface name hash
        std::unordered_map<uint64_t, std::vector<DependencyTrackerInfo>> _dependencyUndoRequestTrackers; // key = interface name hash
        std::unordered_map<CallbackKey, std::function<void(Event const * const)>> _completionCallbacks; // key = listening service id + event type
        std::unordered_map<CallbackKey, std::function<void(Event const * const)>> _errorCallbacks; // key = listening service id + event type
        std::unordered_map<uint64_t, std::vector<EventCallbackInfo>> _eventCallbacks; // key = service id
        IFrameworkLogger *_logger;
        std::multimap<uint64_t, EventStackUniquePtr> _eventQueue;
        std::mutex _eventQueueMutex;
        std::condition_variable _wakeUp;
        std::atomic<uint64_t> _eventIdCounter;
        std::atomic<bool> _quit;
        CommunicationChannel *_communicationChannel;
        uint64_t _id;
        static std::atomic<uint64_t> _managerIdCounter;

        friend class EventCompletionHandlerRegistration;
        friend class CommunicationChannel;
    };
}