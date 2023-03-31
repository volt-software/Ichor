#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/event_queues/IEventQueue.h>

#if defined(__SANITIZE_ADDRESS__)
constexpr uint32_t EVENT_COUNT = 500'000;
#else
constexpr uint32_t EVENT_COUNT = 5'000'000;
#endif

using namespace Ichor;

struct UselessEvent final : public Event {
    explicit UselessEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept :
            Event(TYPE, NAME, _id, _originatingService, _priority) {}
    ~UselessEvent() final = default;

    static constexpr uint64_t TYPE = typeNameHash<UselessEvent>();
    static constexpr std::string_view NAME = typeName<UselessEvent>();
};

class TestService final : public AdvancedService<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IEventQueue>(this, true);
    }
    ~TestService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        auto start = std::chrono::steady_clock::now();
        for(uint32_t i = 0; i < EVENT_COUNT; i++) {
            _q->pushEvent<UselessEvent>(getServiceId());
        }
        auto end = std::chrono::steady_clock::now();
        _q->pushEvent<QuitEvent>(getServiceId());
        ICHOR_LOG_WARN(_logger, "Inserted events in {:L} µs", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());
        co_return {};
    }

    Task<void> stop() final {
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger &logger, IService&) {
        _logger = nullptr;
    }

    void addDependencyInstance(IEventQueue &q, IService&) {
        _q = &q;
    }

    void removeDependencyInstance(IEventQueue &q, IService&) {
        _q = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger{};
    IEventQueue *_q{};
};
