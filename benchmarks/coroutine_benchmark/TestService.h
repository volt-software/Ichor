#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncAutoResetEvent.h>

#if defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
constexpr uint32_t EVENT_COUNT = 10;
#elif defined(__SANITIZE_ADDRESS__)
constexpr uint32_t EVENT_COUNT = 500'000;
#else
constexpr uint32_t EVENT_COUNT = 5'000'000;
#endif

using namespace Ichor;

struct UselessEvent final : public Event {
    explicit UselessEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept :
            Event(_id, _originatingService, _priority) {}
    ~UselessEvent() final = default;

    [[nodiscard]] std::string_view get_name() const noexcept final {
        return NAME;
    }
    [[nodiscard]] NameHashType get_type() const noexcept final {
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
        _q->pushEvent<RunFunctionEventAsync>(getServiceId(), [this]() -> AsyncGenerator<IchorBehaviour> {
            for(uint32_t i = 0; i < EVENT_COUNT; i++) {
                co_await _evt;
                GetThreadLocalEventQueue().pushEvent<RunFunctionEventAsync>(getServiceId(), [this]() -> AsyncGenerator<IchorBehaviour> {
                    _evt.set();
                    co_return {};
                });
            }
            _q->pushEvent<QuitEvent>(getServiceId());
            co_return {};
        });
        _q->pushEvent<RunFunctionEventAsync>(getServiceId(), [this]() -> AsyncGenerator<IchorBehaviour> {
            _evt.set();
            co_return {};
        });
        co_return {};
    }

    Task<void> stop() final {
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger &, IService&) {
        _logger = nullptr;
    }

    void addDependencyInstance(IEventQueue &q, IService&) {
        _q = &q;
    }

    void removeDependencyInstance(IEventQueue &, IService&) {
        _q = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger{};
    IEventQueue *_q{};
    AsyncAutoResetEvent _evt{};
};
