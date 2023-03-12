#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/timer/Timer.h>
#include <atomic>

class Ichor::TimerFactory final : public Ichor::ITimerFactory, public Ichor::AdvancedService<TimerFactory> {
public:
    TimerFactory(Ichor::DependencyRegister &reg, Ichor::Properties props) : Ichor::AdvancedService<TimerFactory>(std::move(props)) {
        reg.registerDependency<Ichor::IEventQueue>(this, true);
        _requestingSvcId = Ichor::any_cast<uint64_t>(getProperties()["requestingSvcId"]);
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

    static std::atomic<uint64_t> _timerIdCounter;
    std::vector<std::unique_ptr<Ichor::Timer>> _timers;
    Ichor::IEventQueue* _queue{};
    uint64_t _requestingSvcId{};
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

    _factories.emplace(evt.originatingService, Ichor::GetThreadLocalManager().createServiceManager<TimerFactory, ITimerFactory>(Properties{{"requestingSvcId", Ichor::make_any<uint64_t>(evt.originatingService)}}));
}

void Ichor::TimerFactoryFactory::handleDependencyUndoRequest(AlwaysNull<ITimerFactory *>, const DependencyUndoRequestEvent &evt) {
    auto factory = _factories.find(evt.originatingService);

    if(factory != _factories.end()) {
        return;
    }

    GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(getServiceId(), factory->second->getServiceId());
    // + 11 because the first stop triggers a dep offline event and inserts a new stop with 10 higher priority.
    GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveServiceEvent>(getServiceId(), INTERNAL_EVENT_PRIORITY + 11, factory->second->getServiceId());
    _factories.erase(factory);
}
