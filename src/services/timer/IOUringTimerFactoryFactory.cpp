#include <ichor/services/timer/IOUringTimerFactoryFactory.h>
#include <ichor/services/timer/TemplatedTimerFactory.h>
#include <ichor/services/timer/IOUringTimer.h>
#include <ichor/events/RunFunctionEvent.h>

Ichor::IOUringTimerFactoryFactory::IOUringTimerFactoryFactory(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<IIOUringQueue>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::IOUringTimerFactoryFactory::start() {
    if(_q->getKernelVersion() < Version{5, 5, 0}) {
        fmt::println("Kernel version too old to use IOUringTcpConnectionService. Requires >= 5.5.0");
        co_return tl::unexpected(StartError::FAILED);
    }

    _trackerRegistration = GetThreadLocalManager().registerDependencyTracker<ITimerFactory>(this, this);

    co_return {};
}

Ichor::Task<void> Ichor::IOUringTimerFactoryFactory::stop() {
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

void Ichor::IOUringTimerFactoryFactory::addDependencyInstance(IIOUringQueue &q, IService&) noexcept {
    _q = &q;
}

void Ichor::IOUringTimerFactoryFactory::removeDependencyInstance(IIOUringQueue&, IService&) noexcept {
    _q = nullptr;
}

void Ichor::IOUringTimerFactoryFactory::handleDependencyRequest(AlwaysNull<ITimerFactory *>, const DependencyRequestEvent &evt) {
    auto factory = _factories.find(evt.originatingService);

    if(factory != _factories.end()) {
        return;
    }

    _factories.emplace(evt.originatingService, Ichor::GetThreadLocalManager().createServiceManager<TimerFactory<IOUringTimer, IIOUringQueue>, Detail::InternalTimerFactory, ITimerFactory>(Properties{{"requestingSvcId", Ichor::make_any<ServiceIdType>(evt.originatingService)}, {"Filter", Ichor::make_any<Filter>(ServiceIdFilterEntry{evt.originatingService})}}, evt.priority)->getServiceId());
}

void Ichor::IOUringTimerFactoryFactory::handleDependencyUndoRequest(AlwaysNull<ITimerFactory *>, const DependencyUndoRequestEvent &evt) {
    auto factory = _factories.find(evt.originatingService);

    if(factory == _factories.end()) {
        return;
    }

    pushStopEventForTimerFactory(factory);
}

decltype(Ichor::IOUringTimerFactoryFactory::_factories)::iterator Ichor::IOUringTimerFactoryFactory::pushStopEventForTimerFactory(decltype(Ichor::IOUringTimerFactoryFactory::_factories)::iterator const &factory) noexcept {
    auto svc = GetThreadLocalManager().getService<Detail::InternalTimerFactory>(factory->second);

    if(!svc) {
        return _factories.erase(factory);
    }

    (*svc).first->stopAllTimers();

    GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, factory->second, true);
    return _factories.erase(factory);
}
