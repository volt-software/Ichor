#pragma once

#include <ichor/Service.h>
#include <ichor/events/Event.h>
#include "../TestEvents.h"

using namespace Ichor;

struct AddEventHandlerDuringEventHandlingService final : public Service<AddEventHandlerDuringEventHandlingService> {
    AddEventHandlerDuringEventHandlingService() = default;

    StartBehaviour start() final {
        _reg = getManager().registerEventHandler<TestEvent>(this);

        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        _reg.reset();
        _reg2.reset();

        return StartBehaviour::SUCCEEDED;
    }

    AsyncGenerator<void> handleEvent(TestEvent const &evt) {
        if(!_addedReg) {
            getManager().createServiceManager<AddEventHandlerDuringEventHandlingService>();
            _reg2 = getManager().registerEventHandler<TestEvent2>(this);
            _addedReg = true;
        }
        co_return;
    }

    AsyncGenerator<void> handleEvent(TestEvent2 const &evt) {
        co_return;
    }

    EventHandlerRegistration _reg{};
    EventHandlerRegistration _reg2{};
    static bool _addedReg;
};