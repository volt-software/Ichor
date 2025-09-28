#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/ServiceExecutionScope.h>
#if defined(ICHOR_ENABLE_INTERNAL_DEBUGGING) || (defined(ICHOR_BUILDING_DEBUG) && (defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)))
constexpr uint32_t SERVICES_COUNT = 100;
#elif defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
constexpr uint32_t SERVICES_COUNT = 1'000;
#else
constexpr uint32_t SERVICES_COUNT = 10'000;
#endif

using namespace Ichor;
using namespace Ichor::v1;

class TestService final : public AdvancedService<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    }
    ~TestService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        auto iteration = Ichor::v1::any_cast<uint64_t>(getProperties()["Iteration"]);
//        fmt::print("Created {} #{} #{}\n", typeName<TestService>(), iteration, SERVICES_COUNT);
        if(iteration == SERVICES_COUNT - 1) {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        }
        co_return {};
    }

    Task<void> stop() final {
        co_return;
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &) {
        _logger = std::move(logger);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*>, IService&) {
        _logger.reset();
    }

    friend DependencyRegister;

    Ichor::ScopedServiceProxy<ILogger*> _logger {};
};
