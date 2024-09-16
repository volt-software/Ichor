#pragma once

#include <chrono>
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/services/serialization/ISerializer.h>
#include "../../examples/common/TestMsg.h"

#if defined(ICHOR_ENABLE_INTERNAL_DEBUGGING) || (defined(ICHOR_BUILDING_DEBUG) && (defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)))
constexpr uint32_t SERDE_COUNT = 1'000;
#elif defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
constexpr uint32_t SERDE_COUNT = 100'000;
#else
constexpr uint32_t SERDE_COUNT = 5'000'000;
#endif

using namespace Ichor;
extern uint64_t sizeof_test;

class TestService final : public AdvancedService<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<ISerializer<TestMsg>>(this, DependencyFlags::REQUIRED);
    }
    ~TestService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        _doWorkRegistration = GetThreadLocalManager().registerEventHandler<DoWorkEvent>(this, this);
        GetThreadLocalEventQueue().pushEvent<DoWorkEvent>(getServiceId());
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "TestService stopped with dependency");
        _doWorkRegistration.reset();
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger&, IService&) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializer<TestMsg> &serializer, IService&) {
        _serializer = &serializer;
        ICHOR_LOG_INFO(_logger, "Inserted serializer");
    }

    void removeDependencyInstance(ISerializer<TestMsg>&, IService&) {
        _serializer = nullptr;
        ICHOR_LOG_INFO(_logger, "Removed serializer");
    }

    AsyncGenerator<IchorBehaviour> handleEvent(DoWorkEvent const &) {
        ICHOR_LOG_ERROR(_logger, "handling DoWorkEvent");
        TestMsg msg{20, "five hundred"};
        sizeof_test = _serializer->serialize(msg).size();
        auto start = std::chrono::steady_clock::now();
        for(uint64_t i = 0; i < SERDE_COUNT; i++) {
            auto res = _serializer->serialize(msg);
            auto msg2 = _serializer->deserialize(res);
            if(msg2->id != msg.id || msg2->val != msg.val) {
                ICHOR_LOG_ERROR(_logger, "serde incorrect!");
            }
        }
        auto end = std::chrono::steady_clock::now();
        ICHOR_LOG_ERROR(_logger, "finished in {:L} µs", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());
        GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        co_return {};
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{};
    ISerializer<TestMsg> *_serializer{};
    EventHandlerRegistration _doWorkRegistration{};
};
