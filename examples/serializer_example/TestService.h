#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/serialization/ISerializer.h>
#include "../common/TestMsg.h"

using namespace Ichor;

class TestService final {
public:
    TestService(DependencyManager *dm, IService *self, ILogger *logger, ISerializer<TestMsg> *serializer) : _dm(dm), _self(self), _logger(logger), _serializer(serializer) {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        _doWorkRegistration = dm->registerEventCompletionCallbacks<DoWorkEvent>(this, self);
        dm->getEventQueue().pushEvent<DoWorkEvent>(self->getServiceId());
    }
    ~TestService() {
        ICHOR_LOG_INFO(_logger, "TestService stopped with dependency");
    }

private:
    void handleCompletion(DoWorkEvent const &) {
        TestMsg msg{20, "five hundred"};
        auto res = _serializer->serialize(msg);
        auto msg2 = _serializer->deserialize(std::move(res));
        if(msg2->id != msg.id || msg2->val != msg.val) {
            ICHOR_LOG_ERROR(_logger, "serde incorrect!");
        } else {
            ICHOR_LOG_ERROR(_logger, "serde correct!");
        }
        _dm->getEventQueue().pushEvent<QuitEvent>(_self->getServiceId());
    }

    void handleError(DoWorkEvent const &evt) {
        ICHOR_LOG_ERROR(_logger, "Error handling DoWorkEvent");
    }

    friend DependencyManager;

    DependencyManager *_dm{};
    IService *_self{};
    ILogger *_logger{};
    ISerializer<TestMsg> *_serializer{};
    EventCompletionHandlerRegistration _doWorkRegistration{};
};
