#pragma once

#include <ichor/Service.h>
#include <ichor/Events.h>

using namespace Ichor;

struct IEventHandlerService {
    virtual ~IEventHandlerService() = default;

    virtual std::unordered_map<uint64_t, uint64_t>& getHandledEvents() = 0;
};

template <Derived<Event> EventT>
struct EventHandlerService final : public IEventHandlerService, public Service<EventHandlerService<EventT>> {

    bool start() final {
        _handler = this->getManager()->template registerEventHandler<EventT>(this);

        return true;
    }

    bool stop() final {
        _handler.reset();

        return true;
    }

    Generator<bool> handleEvent(EventT const * const evt) {
        auto counter = handledEvents.find(evt->type);

        if(counter == end(handledEvents)) {
            handledEvents.template emplace(evt->type, 1);
        } else {
            counter->second++;
        }

        co_return (bool)AllowOthersHandling;
    }

    std::unordered_map<uint64_t, uint64_t>& getHandledEvents() final {
        return handledEvents;
    }

    std::unique_ptr<EventHandlerRegistration, Deleter> _handler{};
    std::unordered_map<uint64_t, uint64_t> handledEvents;
};