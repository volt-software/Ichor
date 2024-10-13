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
    _trackerRegistration.reset();

    for(auto it = _factories.begin(); it != _factories.end(); ) {
        it = pushStopEventForTimerFactory(it);
    }

    if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
        if(!_factories.empty()) {
            fmt::println("_factories not empty. Please file a bug.");
            std::terminate();
        }
    }

    co_return;
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::TimerFactoryFactory::handleDependencyRequest(AlwaysNull<ITimerFactory *>, const DependencyRequestEvent &evt) {
    auto factory = _factories.find(evt.originatingService);

    if(factory != _factories.end()) {
        co_return {};
    }

    _factories.emplace(evt.originatingService, GetThreadLocalManager().createServiceManager<TimerFactory<Timer, IEventQueue>, Detail::InternalTimerFactory, ITimerFactory>(Properties{{"requestingSvcId", Ichor::make_any<ServiceIdType>(evt.originatingService)}, {"Filter", Ichor::make_any<Filter>(ServiceIdFilterEntry{evt.originatingService})}}, evt.priority)->getServiceId());

    co_return {};
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::TimerFactoryFactory::handleDependencyUndoRequest(AlwaysNull<ITimerFactory *>, const DependencyUndoRequestEvent &evt) {
    auto factory = _factories.find(evt.originatingService);

    if(factory == _factories.end()) {
        co_return {};
    }

    pushStopEventForTimerFactory(factory);

    co_return {};
}

decltype(Ichor::TimerFactoryFactory::_factories)::iterator Ichor::TimerFactoryFactory::pushStopEventForTimerFactory(decltype(Ichor::TimerFactoryFactory::_factories)::iterator const &factory) noexcept {
    auto svc = GetThreadLocalManager().getService<Detail::InternalTimerFactory>(factory->second);

    if(!svc) {
        return _factories.erase(factory);
    }

    (*svc).first->stopAllTimers();

    GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, factory->second, true);
    return _factories.erase(factory);
}
