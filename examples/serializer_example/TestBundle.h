#pragma once

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
        auto res = _serializationAdmin->serialize<TestMsg>(msg);
        auto msg2 = _serializationAdmin->deserialize<TestMsg>(res);
        if(msg2->id != msg.id || msg2->val != msg.val) {
            LOG_ERROR(_logger, "serde incorrect!");
        } else {
            LOG_ERROR(_logger, "serde correct!");
        }
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