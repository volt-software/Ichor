#pragma once

#include <ichor/services/timer/TemplatedTimerFactory.h>
#include <ichor/services/timer/IInternalTimerFactory.h>
#include <ichor/DependencyManager.h>
#include <ichor/Filter.h>
#include <vector>

namespace Ichor::v1::Detail {
    /// The shared part of TimerFactoryFactory and IOUringTimerFactoryFactory: creates a TimerFactory<TIMER, QUEUE> for every service
    /// requesting an ITimerFactory, and stops it when that service doesn't need it anymore.
    template <typename TIMER, typename QUEUE>
    class TimerFactoryTracker final {
    public:
        [[nodiscard]] std::vector<ServiceIdType> getCreatedTimerFactoryIds() const noexcept {
            std::vector<ServiceIdType> ret;
            ret.reserve(_factories.size());

            for(auto [_, factoryId] : _factories) {
                ret.emplace_back(factoryId);
            }

            return ret;
        }

        /// After stop(), so that a restarted factory factory creates factories again.
        void start() noexcept {
            _quitting = false;
        }

        void handleDependencyRequest(ServiceIdType selfId, DependencyRequestEvent const &evt) {
            INTERNAL_IO_DEBUG("TimerFactoryTracker {} handleDependencyRequest for {} quit {}", selfId, evt.originatingService, _quitting);

            if(_quitting || _factories.find(evt.originatingService) != _factories.end()) {
                return;
            }

            _factories.emplace(evt.originatingService, GetThreadLocalManager().createServiceManager<TimerFactory<TIMER, QUEUE>, InternalTimerFactory, ITimerFactory>(Properties{{"requestingSvcId", Ichor::v1::make_any<ServiceIdType>(evt.originatingService)}, {"Filter", Ichor::v1::make_any<Filter>(ServiceIdFilterEntry{evt.originatingService})}}, evt.priority)->getServiceId());
        }

        Task<void> handleDependencyUndoRequest(ServiceIdType selfId, DependencyUndoRequestEvent const &evt) {
            INTERNAL_IO_DEBUG("TimerFactoryTracker {} handleDependencyUndoRequest for {} quit {}", selfId, evt.originatingService, _quitting);

            if(_quitting) {
                co_return;
            }

            auto const factoryIt = _factories.find(evt.originatingService);

            if(factoryIt == _factories.cend()) {
                co_return;
            }

            auto const requestingSvcId = factoryIt->first;
            auto const factorySvcId = factoryIt->second;

            co_await stopFactory(selfId, factorySvcId);

            _factories.erase(requestingSvcId);

            co_return;
        }

        /// Stops all created factories, after which no new ones get created.
        Task<void> stop(ServiceIdType selfId) {
            if(_quitting) {
                std::terminate();
            }
            INTERNAL_IO_DEBUG("TimerFactoryTracker {} stop", selfId);

            _quitting = true;

            // _factories is not modified while quitting, but iterators may be invalidated during co_await
            std::vector<std::pair<ServiceIdType, ServiceIdType>> ids;
            ids.reserve(_factories.size());
            for(auto [reqSvcId, factoryId] : _factories) {
                ids.emplace_back(reqSvcId, factoryId);
            }
            for(auto [reqSvcId, factoryId] : ids) {
                co_await stopFactory(selfId, factoryId);
                _factories.erase(reqSvcId);
            }

            if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
                if(!_factories.empty()) {
                    fmt::println("_factories not empty. Please file a bug.");
                    std::terminate();
                }
            }
            INTERNAL_IO_DEBUG("TimerFactoryTracker {} stop done", selfId);

            co_return;
        }

    private:
        static Task<void> stopFactory(ServiceIdType selfId, ServiceIdType factoryId) noexcept {
            auto svc = GetThreadLocalManager().getService<InternalTimerFactory>(factoryId);

            if(!svc) {
                co_return;
            }

            co_await (*svc).first->stopAllTimers();
            GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(selfId, INTERNAL_DEPENDENCY_EVENT_PRIORITY, factoryId, true);

            co_return;
        }

        unordered_map<ServiceIdType, ServiceIdType, ServiceIdHash> _factories;
        bool _quitting{};
    };
}
