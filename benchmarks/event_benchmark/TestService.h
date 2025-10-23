#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/ServiceExecutionScope.h>

#if defined(ICHOR_ENABLE_INTERNAL_DEBUGGING) || (defined(ICHOR_BUILDING_DEBUG) && (defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)))
constexpr uint32_t EVENT_COUNT = 100;
#elif defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
constexpr uint32_t EVENT_COUNT = 500'000;
#else
constexpr uint32_t EVENT_COUNT = 5'000'000;
#endif

using namespace Ichor;
using namespace Ichor::v1;

struct UselessEvent final : public Event {
    constexpr explicit UselessEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority) noexcept :
            Event(_id, _originatingService, _priority) {}
    constexpr ~UselessEvent() final = default;

    [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
        return NAME;
    }
    [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
        return TYPE;
    }

    static constexpr NameHashType TYPE = typeNameHash<UselessEvent>();
    static constexpr std::string_view NAME = typeName<UselessEvent>();
};

class TestService final : public AdvancedService<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<IEventQueue>(this, DependencyFlags::REQUIRED);
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

    void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &) {
        _logger = std::move(logger);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService&) {
        _logger.reset();
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<IEventQueue*> q, IService&) {
        _q = std::move(q);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<IEventQueue*> q, IService&) {
        _q.reset();
    }

    friend DependencyRegister;

    Ichor::ScopedServiceProxy<ILogger*> _logger {};
    Ichor::ScopedServiceProxy<IEventQueue*> _q {};
};
