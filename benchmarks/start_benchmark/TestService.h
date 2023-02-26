#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/event_queues/IEventQueue.h>

#if defined(__SANITIZE_ADDRESS__)
constexpr uint32_t SERVICES_COUNT = 1'000;
#else
constexpr uint32_t SERVICES_COUNT = 10'000;
#endif

using namespace Ichor;

class TestService final : public AdvancedService<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~TestService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        auto iteration = Ichor::any_cast<uint64_t>(getProperties()["Iteration"]);
//        fmt::print("Created {} #{} #{}\n", typeName<TestService>(), iteration, SERVICES_COUNT);
        if(iteration == SERVICES_COUNT - 1) {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        }
        co_return {};
    }

    Task<void> stop() final {
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
