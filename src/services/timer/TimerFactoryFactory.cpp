#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/timer/Timer.h>
#include <atomic>

class Ichor::TimerFactory final : public Ichor::ITimerFactory, public Detail::InternalTimerFactory, public Ichor::AdvancedService<TimerFactory> {
public:
    TimerFactory(Ichor::DependencyRegister &reg, Ichor::Properties props) : Ichor::AdvancedService<TimerFactory>(std::move(props)) {
        reg.registerDependency<Ichor::IEventQueue>(this, DependencyFlags::REQUIRED);
        _requestingSvcId = Ichor::any_cast<ServiceIdType>(getProperties()["requestingSvcId"]);
    }
    ~TimerFactory() final = default;

    Ichor::ITimer& createTimer() final {
#ifdef ICHOR_USE_HARDENING
        if(!isStarted()) {
            std::terminate();
        }
#endif
        return *(_timers.emplace_back(new Ichor::Timer(_queue, _timerIdCounter.fetch_add(1, std::memory_order_relaxed), _requestingSvcId)));
    }

    void destroyTimer(uint64_t timerId) final {
#ifdef ICHOR_USE_HARDENING
        if(!isStarted()) {
            std::terminate();
        }
#endif
        std::erase_if(_timers, [timerId](std::unique_ptr<Ichor::Timer> const &timer) {
            return timer->getTimerId() == timerId;
        });
    }

    void addDependencyInstance(Ichor::IEventQueue &q, IService&) noexcept {
        _queue = &q;
    }

    void removeDependencyInstance(Ichor::IEventQueue&, IService&) noexcept {
        _queue = nullptr;
    }

    void stopAllTimers() noexcept final {
        for(auto& timer : _timers) {
            timer->stopTimer();
        }
    }

    static std::atomic<uint64_t> _timerIdCounter;
    std::vector<std::unique_ptr<Ichor::Timer>> _timers;
    Ichor::IEventQueue* _queue{};
    ServiceIdType _requestingSvcId{};
};
std::atomic<uint64_t> Ichor::TimerFactory::_timerIdCounter{};

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::TimerFactoryFactory::start() {
    _trackerRegistration = GetThreadLocalManager().registerDependencyTracker<ITimerFactory>(this, this);

    co_return {};
}

Ichor::Task<void> Ichor::TimerFactoryFactory::stop() {
    _trackerRegistration.reset();
    co_return;
}

void Ichor::TimerFactoryFactory::handleDependencyRequest(AlwaysNull<ITimerFactory *>, const DependencyRequestEvent &evt) {
    auto factory = _factories.find(evt.originatingService);

    if(factory != _factories.end()) {
        return;
    }

    _factories.emplace(evt.originatingService, Ichor::GetThreadLocalManager().createServiceManager<TimerFactory, Detail::InternalTimerFactory, ITimerFactory>(Properties{{"requestingSvcId", Ichor::make_any<ServiceIdType>(evt.originatingService)}}, evt.priority)->getServiceId());
}

void Ichor::TimerFactoryFactory::handleDependencyUndoRequest(AlwaysNull<ITimerFactory *>, const DependencyUndoRequestEvent &evt) {
    auto factory = _factories.find(evt.originatingService);

    if(factory != _factories.end()) {
        return;
    }

    auto svc = GetThreadLocalManager().getService<Detail::InternalTimerFactory>(factory->second);

    if(!svc) {
        _factories.erase(factory);
        return;
    }

    (*svc).first->stopAllTimers();

    GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, factory->second, true);
    _factories.erase(factory);
}
