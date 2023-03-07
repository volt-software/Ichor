#pragma once

#include <chrono>
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>

#if defined(__SANITIZE_ADDRESS__)
constexpr uint32_t START_STOP_COUNT = 100'000;
#else
constexpr uint32_t START_STOP_COUNT = 1'000'000;
#endif

using namespace Ichor;

class StartStopService final : public AdvancedService<StartStopService> {
public:
    StartStopService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ITestService>(this, true);
        reg.registerDependency<DependencyManager>(this, true);
    }
    ~StartStopService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        if(startCount == 0) {
            _startServiceRegistration = _dm->registerEventCompletionCallbacks<StartServiceEvent>(this);
            _stopServiceRegistration = _dm->registerEventCompletionCallbacks<StopServiceEvent>(this);

            _start = std::chrono::steady_clock::now();
            _dm->getEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, _testServiceId);
        } else if(startCount < START_STOP_COUNT) {
            _dm->getEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, _testServiceId);
        } else {
            auto end = std::chrono::steady_clock::now();
            _dm->getEventQueue().pushEvent<QuitEvent>(getServiceId());
            _startServiceRegistration.reset();
            _stopServiceRegistration.reset();
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

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger &logger, IService&) {
        _logger = nullptr;
    }

    void addDependencyInstance(ITestService&, IService &isvc) {
        _testServiceId = isvc.getServiceId();
    }

    void removeDependencyInstance(ITestService&, IService&) {
    }

    void addDependencyInstance(DependencyManager &dm, IService&) {
        _dm = &dm;
    }

    void removeDependencyInstance(DependencyManager&, IService&) {
    }

    void handleCompletion(StartServiceEvent const &evt) {
    }

    void handleError(StartServiceEvent const &evt) {
    }

    void handleCompletion(StopServiceEvent const &evt) {
    }

    void handleError(StopServiceEvent const &evt) {
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{nullptr};
    DependencyManager *_dm{nullptr};
    uint64_t _testServiceId{0};
    std::chrono::steady_clock::time_point _start{};
    uint64_t startCount{0};
    EventCompletionHandlerRegistration _startServiceRegistration{};
    EventCompletionHandlerRegistration _stopServiceRegistration{};
};
