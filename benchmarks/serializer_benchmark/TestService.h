#pragma once

#include <chrono>
#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "framework/Service.h"
#include "framework/Framework.h"
#include "framework/SerializationAdmin.h"
#include "framework/ServiceLifecycleManager.h"
#include "TestMsg.h"

using namespace Cppelix;


struct ITestService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class TestService : public ITestService, public Service {
public:
    ~TestService() final = default;
    bool start() final {
        LOG_INFO(_logger, "TestService started with dependency");
        _doWorkRegistration = _manager->registerEventCallbacks<DoWorkEvent>(getServiceId(), this);
        _manager->pushEvent<DoWorkEvent>(getServiceId());
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "TestService stopped with dependency");
        return true;
    }

    void addDependencyInstance(IFrameworkLogger *logger) {
        _logger = logger;
        LOG_INFO(_logger, "Inserted logger");
    }

    void removeDependencyInstance(IFrameworkLogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = serializationAdmin;
        LOG_INFO(_logger, "Inserted serializationAdmin");
    }

    void removeDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = nullptr;
        LOG_INFO(_logger, "Removed serializationAdmin");
    }

    void handleCompletion(DoWorkEvent const * const evt) {
        TestMsg msg{20, "five hundred"};
        auto start = std::chrono::system_clock::now();
        for(uint64_t i = 0; i < 1'000'000; i++) {
            auto res = _serializationAdmin->serialize<TestMsg>(msg);
            auto msg2 = _serializationAdmin->deserialize<TestMsg>(std::move(res));
            if(msg2->id != msg.id || msg2->val != msg.val) {
                LOG_ERROR(_logger, "serde incorrect!");
            }
        }
        auto end = std::chrono::system_clock::now();
        LOG_INFO(_logger, "finished in {:n} µs", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());
        _manager->pushEvent<QuitEvent>(getServiceId());
    }

    void handleError(DoWorkEvent const * const evt) {
        LOG_ERROR(_logger, "Error handling DoWorkEvent");
    }

private:
    IFrameworkLogger *_logger;
    ISerializationAdmin *_serializationAdmin;
    std::unique_ptr<EventHandlerRegistration> _doWorkRegistration;
};