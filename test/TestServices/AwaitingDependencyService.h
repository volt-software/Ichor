#pragma once
extern std::atomic<uint64_t> evtGate;
extern std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;

struct AwaitingDependencyService final : public AdvancedService<AwaitingDependencyService> {
    AwaitingDependencyService(DependencyRegister &reg, Properties props)
        : AdvancedService(std::move(props)) {
        reg.registerDependency<IUselessService>(this, DependencyFlags::REQUIRED);
    }

    Task<tl::expected<void, StartError>> start() final { co_return {}; }

    Task<void> stop() final {
        fmt::println("1");
        evtGate.fetch_add(1, std::memory_order_release);
        fmt::println("2");
        co_await *_evt;
        fmt::println("3");
        co_return;
    }

    void addDependencyInstance(ScopedServiceProxy<IUselessService*>, IService&) {}
    void removeDependencyInstance(ScopedServiceProxy<IUselessService*>, IService&) {}
};
