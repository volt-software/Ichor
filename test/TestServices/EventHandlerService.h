#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/events/Event.h>

using namespace Ichor;
using namespace Ichor::v1;

struct IEventHandlerService {
    virtual std::unordered_map<uint64_t, uint64_t>& getHandledEvents() = 0;

protected:
    ~IEventHandlerService() = default;
};

template <Derived<Event> EventT>
struct EventHandlerService final : public IEventHandlerService, public AdvancedService<EventHandlerService<EventT>> {
    EventHandlerService() = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        _handler = GetThreadLocalManager().template registerEventHandler<EventT>(this, this);

        co_return {};
    }

    Task<void> stop() final {
        _handler.reset();

        co_return;
    }

    AsyncGenerator<IchorBehaviour> handleEvent(EventT const &evt) {
        auto evtType = evt.get_type();
        auto counter = handledEvents.find(evtType);

        if(counter == end(handledEvents)) {
            handledEvents.template emplace<>(evtType, 1);
        } else {
            counter->second++;
        }

        co_return {};
    }

    std::unordered_map<uint64_t, uint64_t>& getHandledEvents() final {
        return handledEvents;
    }

    EventHandlerRegistration _handler{};
    std::unordered_map<uint64_t, uint64_t> handledEvents;
};
