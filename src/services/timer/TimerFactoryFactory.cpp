#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/services/timer/Timer.h>
#include <atomic>

std::atomic<uint64_t> Ichor::v1::_timerIdCounter{1};

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::v1::TimerFactoryFactory::start() {
    _tracker.start();
    _trackerRegistration = GetThreadLocalManager().registerDependencyTracker<ITimerFactory>(this, this);

    co_return {};
}

Ichor::Task<void> Ichor::v1::TimerFactoryFactory::stop() {
    _trackerRegistration.reset();
    co_await _tracker.stop(getServiceId());

    co_return;
}

std::vector<Ichor::ServiceIdType> Ichor::v1::TimerFactoryFactory::getCreatedTimerFactoryIds() const noexcept {
    return _tracker.getCreatedTimerFactoryIds();
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::v1::TimerFactoryFactory::handleDependencyRequest(v1::AlwaysNull<ITimerFactory *>, const DependencyRequestEvent &evt) {
    _tracker.handleDependencyRequest(getServiceId(), evt);

    co_return {};
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::v1::TimerFactoryFactory::handleDependencyUndoRequest(v1::AlwaysNull<ITimerFactory *>, const DependencyUndoRequestEvent &evt) {
    co_await _tracker.handleDependencyUndoRequest(getServiceId(), evt);

    co_return {};
}
