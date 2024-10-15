#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/services/timer/TemplatedTimerFactory.h>
#include <ichor/services/timer/Timer.h>
#include <ichor/events/RunFunctionEvent.h>
#include <atomic>

std::atomic<uint64_t> Ichor::_timerIdCounter{1};

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::TimerFactoryFactory::start() {
    _trackerRegistration = GetThreadLocalManager().registerDependencyTracker<ITimerFactory>(this, this);

    co_return {};
}

Ichor::Task<void> Ichor::TimerFactoryFactory::stop() {
    if(_quitting) {
        std::terminate();
    }
    INTERNAL_IO_DEBUG("TimerFactoryFactory {} stop", getServiceId());

    _trackerRegistration.reset();
    _quitting = true;

    if(!_factories.empty()) {
        std::vector<std::pair<ServiceIdType, ServiceIdType>> ids;
        ids.reserve(_factories.size());
        for(auto [reqSvcId, factoryId] : _factories) {
            ids.emplace_back(reqSvcId, factoryId);
        }
        for(auto [reqSvcId, factoryId] : ids) {
            co_await pushStopEventForTimerFactory(reqSvcId, factoryId);
            _factories.erase(reqSvcId);
        }
    }

    if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
        if(!_factories.empty()) {
            fmt::println("_factories not empty. Please file a bug.");
            std::terminate();
        }
    }
    INTERNAL_IO_DEBUG("TimerFactoryFactory {} stop done", getServiceId());

    co_return;
}

std::vector<Ichor::ServiceIdType> Ichor::TimerFactoryFactory::getCreatedTimerFactoryIds() const noexcept {
    std::vector<ServiceIdType> ret;
    ret.reserve(_factories.size());

    for(auto [_, factoryId] : _factories) {
        ret.emplace_back(factoryId);
    }

    return ret;
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::TimerFactoryFactory::handleDependencyRequest(AlwaysNull<ITimerFactory *>, const DependencyRequestEvent &evt) {
    INTERNAL_IO_DEBUG("TimerFactoryFactory {} handleDependencyRequest for {} quit {}", getServiceId(), evt.originatingService, _quitting);

    if(_quitting) {
        INTERNAL_IO_DEBUG("TimerFactoryFactory {} handleDependencyRequest for {} done1", getServiceId(), evt.originatingService, _quitting);
        co_return {};
    }

    auto factory = _factories.find(evt.originatingService);

    if(factory != _factories.end()) {
        INTERNAL_IO_DEBUG("TimerFactoryFactory {} handleDependencyRequest for {} done2", getServiceId(), evt.originatingService, _quitting);
        co_return {};
    }

    _factories.emplace(evt.originatingService, GetThreadLocalManager().createServiceManager<TimerFactory<Timer, IEventQueue>, Detail::InternalTimerFactory, ITimerFactory>(Properties{{"requestingSvcId", Ichor::make_any<ServiceIdType>(evt.originatingService)}, {"Filter", Ichor::make_any<Filter>(ServiceIdFilterEntry{evt.originatingService})}}, evt.priority)->getServiceId());

    INTERNAL_IO_DEBUG("TimerFactoryFactory {} handleDependencyRequest for {} done3", getServiceId(), evt.originatingService, _quitting);
    co_return {};
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::TimerFactoryFactory::handleDependencyUndoRequest(AlwaysNull<ITimerFactory *>, const DependencyUndoRequestEvent &evt) {
    INTERNAL_IO_DEBUG("TimerFactoryFactory {} handleDependencyUndoRequest for {} quit {}", getServiceId(), evt.originatingService, _quitting);

    if(_quitting) {
        INTERNAL_IO_DEBUG("TimerFactoryFactory {} handleDependencyUndoRequest for {} done1", getServiceId(), evt.originatingService, _quitting);
        co_return {};
    }

    auto factory = _factories.find(evt.originatingService);

    if(factory == _factories.cend()) {
        INTERNAL_IO_DEBUG("TimerFactoryFactory {} handleDependencyUndoRequest for {} done2", getServiceId(), evt.originatingService, _quitting);
        co_return {};
    }

    INTERNAL_IO_DEBUG("TimerFactoryFactory {} handleDependencyUndoRequest for {} pre-co_await", getServiceId(), evt.originatingService, _quitting);

    co_await pushStopEventForTimerFactory(factory->first, factory->second);

    _factories.erase(evt.originatingService);

    INTERNAL_IO_DEBUG("TimerFactoryFactory {} handleDependencyUndoRequest for {} done3", getServiceId(), evt.originatingService, _quitting);

    co_return {};
}

Ichor::Task<void> Ichor::TimerFactoryFactory::pushStopEventForTimerFactory(ServiceIdType requestingSvcId, ServiceIdType factoryId) noexcept {
    INTERNAL_IO_DEBUG("TimerFactoryFactory {} pushStopEventForTimerFactory for {}", getServiceId(), requestingSvcId);
    auto svc = GetThreadLocalManager().getService<Detail::InternalTimerFactory>(factoryId);

    if(!svc) {
        INTERNAL_IO_DEBUG("TimerFactoryFactory {} pushStopEventForTimerFactory for {} done1", getServiceId(), requestingSvcId);
        co_return;
    }

    INTERNAL_IO_DEBUG("TimerFactoryFactory {} pushStopEventForTimerFactory for {} pre-co_await", getServiceId(), requestingSvcId);
    // iterator may be invalidated after co_await.
    co_await (*svc).first->stopAllTimers();
    GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, factoryId, true);
    INTERNAL_IO_DEBUG("TimerFactoryFactory {} pushStopEventForTimerFactory for {} done2", getServiceId(), requestingSvcId);

    co_return;
}
