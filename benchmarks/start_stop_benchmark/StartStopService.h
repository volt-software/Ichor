#pragma once

#include <chrono>
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/ServiceExecutionScope.h>

#if defined(ICHOR_ENABLE_INTERNAL_DEBUGGING) || (defined(ICHOR_BUILDING_DEBUG) && (defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)))
constexpr uint32_t START_STOP_COUNT = 100;
#elif defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
constexpr uint32_t START_STOP_COUNT = 100'000;
#else
constexpr uint32_t START_STOP_COUNT = 1'000'000;
#endif

using namespace Ichor;
using namespace Ichor::v1;

class StartStopService final : public AdvancedService<StartStopService> {
public:
    StartStopService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<ITestService>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<DependencyManager>(this, DependencyFlags::REQUIRED);
    }
    ~StartStopService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        if(startCount == 0) {
            _start = std::chrono::steady_clock::now();
            _dm->getEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, _testServiceId);
        } else if(startCount < START_STOP_COUNT) {
            _dm->getEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, _testServiceId);
        } else {
            auto end = std::chrono::steady_clock::now();
            _dm->getEventQueue().pushEvent<QuitEvent>(getServiceId());
            ICHOR_LOG_INFO(_logger, "dm {} finished in {:L} µs", _dm->getId(), std::chrono::duration_cast<std::chrono::microseconds>(end-_start).count());
        }
        startCount++;
        co_return {};
    }

    Task<void> stop() final {
        if(startCount <= START_STOP_COUNT) {
            _dm->getEventQueue().pushEvent<StartServiceEvent>(getServiceId(), _testServiceId);
        }
        co_return;
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &) {
        _logger = std::move(logger);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService&) {
        _logger.reset();
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ITestService*>, IService &isvc) {
        _testServiceId = isvc.getServiceId();
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ITestService*>, IService&) {
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<DependencyManager*> dm, IService&) {
        _dm = std::move(dm);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<DependencyManager*>, IService&) {
    }

    friend DependencyRegister;
    friend DependencyManager;

    Ichor::ScopedServiceProxy<ILogger*> _logger {};
    Ichor::ScopedServiceProxy<DependencyManager*> _dm {};
    uint64_t _testServiceId{0};
    std::chrono::steady_clock::time_point _start{};
    uint64_t startCount{0};
};
