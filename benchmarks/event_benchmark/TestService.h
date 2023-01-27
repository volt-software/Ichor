#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/Service.h>
#include <ichor/dependency_management/ILifecycleManager.h>

#if defined(__SANITIZE_ADDRESS__)
constexpr uint32_t EVENT_COUNT = 500'000;
#else
constexpr uint32_t EVENT_COUNT = 5'000'000;
#endif

using namespace Ichor;

struct UselessEvent final : public Event {
    explicit UselessEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept :
            Event(TYPE, NAME, _id, _originatingService, _priority) {}
    ~UselessEvent() final = default;

    static constexpr uint64_t TYPE = typeNameHash<UselessEvent>();
    static constexpr std::string_view NAME = typeName<UselessEvent>();
};

class TestService final : public Service<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~TestService() final = default;

private:
    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        auto start = std::chrono::steady_clock::now();
        for(uint32_t i = 0; i < EVENT_COUNT; i++) {
            getManager().pushEvent<UselessEvent>(getServiceId());
        }
        auto end = std::chrono::steady_clock::now();
        getManager().pushEvent<QuitEvent>(getServiceId());
        ICHOR_LOG_WARN(_logger, "Inserted events in {:L} µs", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());
        co_return {};
    }

    AsyncGenerator<void> stop() final {
        co_return;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger{nullptr};
};
