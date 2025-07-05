#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/events/Event.h>
#include "../TestEvents.h"

using namespace Ichor;
using namespace Ichor::v1;

struct AddEventHandlerDuringEventHandlingService final : public AdvancedService<AddEventHandlerDuringEventHandlingService> {
    AddEventHandlerDuringEventHandlingService() = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        _reg = GetThreadLocalManager().registerEventHandler<TestEvent>(this, this);

        co_return {};
    }

    Task<void> stop() final {
        _reg.reset();
        _reg2.reset();

        co_return;
    }

    AsyncGenerator<IchorBehaviour> handleEvent(TestEvent const &evt) {
        if(!_addedReg) {
            GetThreadLocalManager().createServiceManager<AddEventHandlerDuringEventHandlingService>();
            _reg2 = GetThreadLocalManager().registerEventHandler<TestEvent2>(this, this);
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
