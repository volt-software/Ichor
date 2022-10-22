#pragma once

#include <ichor/Service.h>
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
    virtual AsyncGenerator<bool> await_something() = 0;
};
struct AwaitService final : public IAwaitService, public Service<AwaitService> {
    AwaitService() = default;
    ~AwaitService() final = default;

    AsyncGenerator<bool> await_something() final
    {
        co_await *_evt;
        co_return (bool)PreventOthersHandling;
    }
};
struct EventAwaitService final : public Service<EventAwaitService> {
    EventAwaitService() = default;
    ~EventAwaitService() final = default;
    StartBehaviour start() final {
        _handler = this->getManager()->template registerEventHandler<AwaitEvent>(this);

        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        _handler.reset();

        return StartBehaviour::SUCCEEDED;
    }

    AsyncGenerator<bool> handleEvent(AwaitEvent const &evt) {
        co_await *_evt;
        co_yield false;

        this->getManager()->pushEvent<QuitEvent>(getServiceId());

        co_return (bool)AllowOthersHandling;
    }

    EventHandlerRegistration _handler{};
};