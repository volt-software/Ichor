#pragma once

#include <chrono>
#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "framework/Bundle.h"
#include "framework/Framework.h"
#include "framework/SerializationAdmin.h"
#include "framework/ComponentLifecycleManager.h"
#include "TestMsg.h"

using namespace Cppelix;


struct ITestBundle : virtual public IBundle {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class TestBundle : public ITestBundle, public Bundle {
public:
    ~TestBundle() final = default;
    bool start() final {
        LOG_INFO(_logger, "TestBundle started with dependency");
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "TestBundle stopped with dependency");
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
        _manager->PushEvent<QuitEvent>();
    }

    void removeDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = nullptr;
        LOG_INFO(_logger, "Removed serializationAdmin");
    }

private:
    IFrameworkLogger *_logger;
    ISerializationAdmin *_serializationAdmin;
};