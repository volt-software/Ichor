#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/events/Event.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>

using namespace Ichor;

extern std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;

struct AwaitEvent final : public Event {
    AwaitEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority) {}
    ~AwaitEvent() final = default;

    static constexpr uint64_t TYPE = typeNameHash<AwaitEvent>();
    static constexpr std::string_view NAME = typeName<AwaitEvent>();
};

struct IAwaitService {
    virtual ~IAwaitService() = default;
    virtual AsyncGenerator<void> await_something() = 0;
};
struct AwaitService final : public IAwaitService, public AdvancedService<AwaitService> {
    AwaitService() = default;
    ~AwaitService() final = default;

    AsyncGenerator<void> await_something() final
    {
        co_await *_evt;
        co_return;
    }
};
struct EventAwaitService final : public AdvancedService<EventAwaitService> {
    EventAwaitService() = default;
    ~EventAwaitService() final = default;
    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        _handler = this->getManager().template registerEventHandler<AwaitEvent>(this);

        co_return {};
    }

    AsyncGenerator<void> stop() final {
        _handler.reset();

        co_return;
    }

    AsyncGenerator<IchorBehaviour> handleEvent(AwaitEvent const &evt) {
        co_await *_evt;
        co_yield {};

        this->getManager().pushEvent<QuitEvent>(getServiceId());

        co_return {};
    }

    EventHandlerRegistration _handler{};
};
