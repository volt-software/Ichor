#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/events/Event.h>
#include "../TestEvents.h"

using namespace Ichor;

struct AddEventHandlerDuringEventHandlingService final : public AdvancedService<AddEventHandlerDuringEventHandlingService> {
    AddEventHandlerDuringEventHandlingService() = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        _reg = getManager().registerEventHandler<TestEvent>(this);

        co_return {};
    }

    Task<void> stop() final {
        _reg.reset();
        _reg2.reset();

        co_return;
    }

    AsyncGenerator<IchorBehaviour> handleEvent(TestEvent const &evt) {
        if(!_addedReg) {
            getManager().createServiceManager<AddEventHandlerDuringEventHandlingService>();
            _reg2 = getManager().registerEventHandler<TestEvent2>(this);
            _addedReg = true;
        }
        co_return {};
    }

    AsyncGenerator<IchorBehaviour> handleEvent(TestEvent2 const &evt) {
        co_return {};
    }

    EventHandlerRegistration _reg{};
    EventHandlerRegistration _reg2{};
    static bool _addedReg;
};
