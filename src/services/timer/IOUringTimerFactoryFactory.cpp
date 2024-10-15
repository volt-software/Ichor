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
    _quitting = true;

    if(!_factories.empty()) {
        std::vector<std::pair<ServiceIdType, ServiceIdType>> ids;
        ids.reserve(_factories.size());
        for(auto [reqSvcId, factoryId] : _factories) {
            ids.emplace_back(reqSvcId, factoryId);
        }
        for(auto [reqSvcId, factoryId] : ids) {
//            fmt::println("TimerFactoryFactory extracting {}", reqSvcId);
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

    co_return;
}

void Ichor::IOUringTimerFactoryFactory::addDependencyInstance(IIOUringQueue &q, IService&) noexcept {
    _q = &q;
}

void Ichor::IOUringTimerFactoryFactory::removeDependencyInstance(IIOUringQueue&, IService&) noexcept {
    _q = nullptr;
}

std::vector<Ichor::ServiceIdType> Ichor::IOUringTimerFactoryFactory::getCreatedTimerFactoryIds() const noexcept {
    std::vector<ServiceIdType> ret;
    ret.reserve(_factories.size());

    for(auto [_, factoryId] : _factories) {
        ret.emplace_back(factoryId);
    }

    return ret;
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::IOUringTimerFactoryFactory::handleDependencyRequest(AlwaysNull<ITimerFactory *>, const DependencyRequestEvent &evt) {
    if(_quitting) {
        co_return {};
    }

    auto factory = _factories.find(evt.originatingService);

    if(factory != _factories.end()) {
        co_return {};
    }

    _factories.emplace(evt.originatingService, Ichor::GetThreadLocalManager().createServiceManager<TimerFactory<IOUringTimer, IIOUringQueue>, Detail::InternalTimerFactory, ITimerFactory>(Properties{{"requestingSvcId", Ichor::make_any<ServiceIdType>(evt.originatingService)}, {"Filter", Ichor::make_any<Filter>(ServiceIdFilterEntry{evt.originatingService})}}, evt.priority)->getServiceId());

    co_return {};
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::IOUringTimerFactoryFactory::handleDependencyUndoRequest(AlwaysNull<ITimerFactory *>, const DependencyUndoRequestEvent &evt) {
    if(_quitting) {
        co_return {};
    }

    auto factory = _factories.find(evt.originatingService);

    if(factory == _factories.cend()) {
        co_return {};
    }

    co_await pushStopEventForTimerFactory(factory->first, factory->second);

    _factories.erase(evt.originatingService);

    co_return {};
}

Ichor::Task<void> Ichor::IOUringTimerFactoryFactory::pushStopEventForTimerFactory(ServiceIdType requestingSvcId, ServiceIdType factoryId) noexcept {
    auto svc = GetThreadLocalManager().getService<Detail::InternalTimerFactory>(factoryId);

    if(!svc) {
        co_return;
    }

    // iterator may be invalidated after co_await.
    co_await (*svc).first->stopAllTimers();
    GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, factoryId, true);

    co_return;
}
