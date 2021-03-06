#pragma once

#include <chrono>
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/optional_bundles/serialization_bundle/ISerializationAdmin.h>
#include <ichor/LifecycleManager.h>
#include "../../examples/common/TestMsg.h"

using namespace Ichor;

class TestService final : public Service<TestService> {
public:
    TestService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ISerializationAdmin>(this, true);
    }
    ~TestService() final = default;
    bool start() final {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        _doWorkRegistration = getManager()->registerEventCompletionCallbacks<DoWorkEvent>(this);
        getManager()->pushEvent<DoWorkEvent>(getServiceId());
        return true;
    }

    bool stop() final {
        ICHOR_LOG_INFO(_logger, "TestService stopped with dependency");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
        ICHOR_LOG_TRACE(_logger, "Inserted logger");
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = serializationAdmin;
        ICHOR_LOG_INFO(_logger, "Inserted serializationAdmin");
    }

    void removeDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = nullptr;
        ICHOR_LOG_INFO(_logger, "Removed serializationAdmin");
    }

    void handleCompletion(DoWorkEvent const * const evt) {
        TestMsg msg{20, "five hundred"};
        auto start = std::chrono::steady_clock::now();
        for(uint64_t i = 0; i < 1'000'000; i++) {
            auto res = _serializationAdmin->serialize<TestMsg>(msg);
            auto msg2 = _serializationAdmin->deserialize<TestMsg>(std::move(res));
            if(msg2->id != msg.id || msg2->val != msg.val) {
                ICHOR_LOG_ERROR(_logger, "serde incorrect!");
            }
        }
        auto end = std::chrono::steady_clock::now();
        ICHOR_LOG_INFO(_logger, "finished in {:L} µs", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());
        getManager()->pushEvent<QuitEvent>(getServiceId());
    }

    void handleError(DoWorkEvent const * const evt) {
        ICHOR_LOG_ERROR(_logger, "Error handling DoWorkEvent");
    }

private:
    ILogger *_logger;
    ISerializationAdmin *_serializationAdmin;
    std::unique_ptr<EventCompletionHandlerRegistration> _doWorkRegistration;
};