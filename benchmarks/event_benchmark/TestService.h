#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

#ifdef __SANITIZE_ADDRESS__
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
    StartBehaviour start() final {
        auto start = std::chrono::steady_clock::now();
        for(uint32_t i = 0; i < EVENT_COUNT; i++) {
            getManager().pushEvent<UselessEvent>(getServiceId());
        }
        auto end = std::chrono::steady_clock::now();
        getManager().pushEvent<QuitEvent>(getServiceId());
        ICHOR_LOG_WARN(_logger, "Inserted events in {:L} µs", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());
        return Ichor::StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        return Ichor::StartBehaviour::SUCCEEDED;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

private:
    ILogger *_logger{nullptr};
};