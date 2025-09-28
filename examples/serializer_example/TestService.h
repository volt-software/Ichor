#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/ServiceExecutionScope.h>
#include "../common/TestMsg.h"

using namespace Ichor;
using namespace Ichor::v1;

class TestService final {
public:
    TestService(DependencyManager *dm, IService *self, ScopedServiceProxy<ILogger> logger, ScopedServiceProxy<ISerializer<TestMsg>> serializer) : _dm(dm), _self(self), _logger(logger), _serializer(serializer) {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        _doWorkRegistration = dm->registerEventHandler<DoWorkEvent>(this, self);
        dm->getEventQueue().pushEvent<DoWorkEvent>(self->getServiceId());
    }
    ~TestService() {
        ICHOR_LOG_INFO(_logger, "TestService stopped with dependency");
    }

private:
    AsyncGenerator<IchorBehaviour> handleEvent(DoWorkEvent const &) {
        TestMsg msg{20, "five hundred"};
        auto res = _serializer->serialize(msg);
        auto msg2 = _serializer->deserialize(res);
        if(msg2->id != msg.id || msg2->val != msg.val) {
            ICHOR_LOG_ERROR(_logger, "serde incorrect!");
        } else {
            ICHOR_LOG_ERROR(_logger, "serde correct!");
        }
        _dm->getEventQueue().pushEvent<QuitEvent>(_self->getServiceId());
        co_return {};
    }

    friend DependencyManager;

    DependencyManager *_dm{};
    IService *_self{};
    ScopedServiceProxy<ILogger> _logger{};
    ScopedServiceProxy<ISerializer<TestMsg>> _serializer{};
    EventHandlerRegistration _doWorkRegistration{};
};
