#include <ichor/services/timer/IOUringTimerFactoryFactory.h>
#include <ichor/services/timer/IOUringTimer.h>
#include <ichor/ScopedServiceProxy.h>

Ichor::v1::IOUringTimerFactoryFactory::IOUringTimerFactoryFactory(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<IIOUringQueue>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::v1::IOUringTimerFactoryFactory::start() {
    if(_q->getKernelVersion() < Version{.major=5, .minor=5, .patch=0}) {
        fmt::println("Kernel version too old to use IOUringTimerFactoryFactory. Requires >= 5.5.0");
        co_return tl::unexpected(StartError::FAILED);
    }

    _tracker.start();
    _trackerRegistration = GetThreadLocalManager().registerDependencyTracker<ITimerFactory>(this, this);

    co_return {};
}

Ichor::Task<void> Ichor::v1::IOUringTimerFactoryFactory::stop() {
    _trackerRegistration.reset();
    co_await _tracker.stop(getServiceId());

    co_return;
}

void Ichor::v1::IOUringTimerFactoryFactory::addDependencyInstance(Ichor::ScopedServiceProxy<IIOUringQueue*> q, IService&) noexcept {
    _q = std::move(q);
}

void Ichor::v1::IOUringTimerFactoryFactory::removeDependencyInstance(Ichor::ScopedServiceProxy<IIOUringQueue*>, IService&) noexcept {
    _q = nullptr;
}

std::vector<Ichor::ServiceIdType> Ichor::v1::IOUringTimerFactoryFactory::getCreatedTimerFactoryIds() const noexcept {
    return _tracker.getCreatedTimerFactoryIds();
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::v1::IOUringTimerFactoryFactory::handleDependencyRequest(v1::AlwaysNull<ITimerFactory *>, const DependencyRequestEvent &evt) {
    _tracker.handleDependencyRequest(getServiceId(), evt);

    co_return {};
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::v1::IOUringTimerFactoryFactory::handleDependencyUndoRequest(v1::AlwaysNull<ITimerFactory *>, const DependencyUndoRequestEvent &evt) {
    co_await _tracker.handleDependencyUndoRequest(getServiceId(), evt);

    co_return {};
}
