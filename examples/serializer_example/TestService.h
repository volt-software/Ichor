#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/Service.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/dependency_management/ILifecycleManager.h>
#include "../common/TestMsg.h"

using namespace Ichor;

class TestService final : public Service<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ISerializer<TestMsg>>(this, true);
    }
    ~TestService() final = default;

private:
    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        _doWorkRegistration = getManager().registerEventCompletionCallbacks<DoWorkEvent>(this);
        getManager().pushEvent<DoWorkEvent>(getServiceId());
        co_return {};
    }

    AsyncGenerator<void> stop() final {
        ICHOR_LOG_INFO(_logger, "TestService stopped with dependency");
        _doWorkRegistration.reset();
        co_return;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
        ICHOR_LOG_TRACE(_logger, "Inserted logger");
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializer<TestMsg> *serializer, IService *) {
        _serializer = serializer;
        ICHOR_LOG_INFO(_logger, "Inserted serializer");
    }

    void removeDependencyInstance(ISerializer<TestMsg> *serializer, IService *) {
        _serializer = nullptr;
        ICHOR_LOG_INFO(_logger, "Removed serializer");
    }

    void handleCompletion(DoWorkEvent const &evt) {
        TestMsg msg{20, "five hundred"};
        auto res = _serializer->serialize(msg);
        auto msg2 = _serializer->deserialize(std::move(res));
        if(msg2->id != msg.id || msg2->val != msg.val) {
            ICHOR_LOG_ERROR(_logger, "serde incorrect!");
        } else {
            ICHOR_LOG_ERROR(_logger, "serde correct!");
        }
        getManager().pushEvent<QuitEvent>(getServiceId());
    }

    void handleError(DoWorkEvent const &evt) {
        ICHOR_LOG_ERROR(_logger, "Error handling DoWorkEvent");
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{};
    ISerializer<TestMsg> *_serializer{};
    EventCompletionHandlerRegistration _doWorkRegistration{};
};
