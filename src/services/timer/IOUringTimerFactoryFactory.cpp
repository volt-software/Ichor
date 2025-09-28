#include <ichor/services/timer/IOUringTimerFactoryFactory.h>
#include <ichor/services/timer/TemplatedTimerFactory.h>
#include <ichor/services/timer/IOUringTimer.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/Filter.h>
#include <ichor/ServiceExecutionScope.h>

Ichor::v1::IOUringTimerFactoryFactory::IOUringTimerFactoryFactory(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<IIOUringQueue>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::v1::IOUringTimerFactoryFactory::start() {
    if(_q->getKernelVersion() < Version{5, 5, 0}) {
        fmt::println("Kernel version too old to use IOUringTcpConnectionService. Requires >= 5.5.0");
        co_return tl::unexpected(StartError::FAILED);
    }

    _trackerRegistration = GetThreadLocalManager().registerDependencyTracker<ITimerFactory>(this, this);

    co_return {};
}

Ichor::Task<void> Ichor::v1::IOUringTimerFactoryFactory::stop() {
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

void Ichor::v1::IOUringTimerFactoryFactory::addDependencyInstance(Ichor::ScopedServiceProxy<IIOUringQueue*> q, IService&) noexcept {
    _q = std::move(q);
}

void Ichor::v1::IOUringTimerFactoryFactory::removeDependencyInstance(Ichor::ScopedServiceProxy<IIOUringQueue*>, IService&) noexcept {
    _q = nullptr;
}

std::vector<Ichor::ServiceIdType> Ichor::v1::IOUringTimerFactoryFactory::getCreatedTimerFactoryIds() const noexcept {
    std::vector<ServiceIdType> ret;
    ret.reserve(_factories.size());

    for(auto [_, factoryId] : _factories) {
        ret.emplace_back(factoryId);
    }

    return ret;
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::v1::IOUringTimerFactoryFactory::handleDependencyRequest(v1::AlwaysNull<ITimerFactory *>, const DependencyRequestEvent &evt) {
    if(_quitting) {
        co_return {};
    }

    auto factory = _factories.find(evt.originatingService);

    if(factory != _factories.end()) {
        co_return {};
    }

    _factories.emplace(evt.originatingService, GetThreadLocalManager().createServiceManager<TimerFactory<IOUringTimer, IIOUringQueue>, Ichor::Detail::v1::InternalTimerFactory, ITimerFactory>(Properties{{"requestingSvcId", Ichor::v1::make_any<ServiceIdType>(evt.originatingService)}, {"Filter", Ichor::v1::make_any<Filter>(ServiceIdFilterEntry{evt.originatingService})}}, evt.priority)->getServiceId());

    co_return {};
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::v1::IOUringTimerFactoryFactory::handleDependencyUndoRequest(v1::AlwaysNull<ITimerFactory *>, const DependencyUndoRequestEvent &evt) {
    if(_quitting) {
        co_return {};
    }

    auto const factoryIt = _factories.find(evt.originatingService);

    if(factoryIt == _factories.cend()) {
        co_return {};
    }

    auto const requestingSvcId = factoryIt->first;
    auto const factorySvcId = factoryIt->second;

    co_await pushStopEventForTimerFactory(requestingSvcId, factorySvcId);

    _factories.erase(requestingSvcId);

    co_return {};
}

Ichor::Task<void> Ichor::v1::IOUringTimerFactoryFactory::pushStopEventForTimerFactory(ServiceIdType requestingSvcId, ServiceIdType factoryId) noexcept {
    auto svc = GetThreadLocalManager().getService<Ichor::Detail::v1::InternalTimerFactory>(factoryId);

    if(!svc) {
        co_return;
    }

    // iterator may be invalidated after co_await.
    co_await (*svc).first->stopAllTimers();
    GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, factoryId, true);

    co_return;
}
