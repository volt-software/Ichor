#pragma once

#include <chrono>
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

#ifdef __SANITIZE_ADDRESS__
constexpr uint32_t START_STOP_COUNT = 100'000;
#else
constexpr uint32_t START_STOP_COUNT = 1'000'000;
#endif

using namespace Ichor;

class StartStopService final : public Service<StartStopService> {
public:
    StartStopService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ITestService>(this, true);
    }
    ~StartStopService() final = default;
    StartBehaviour start() final {
        if(startCount == 0) {
            _startServiceRegistration = getManager()->registerEventCompletionCallbacks<StartServiceEvent>(this);
            _stopServiceRegistration = getManager()->registerEventCompletionCallbacks<StopServiceEvent>(this);

            _start = std::chrono::steady_clock::now();
            getManager()->pushEvent<StopServiceEvent>(getServiceId(), _testServiceId);
        } else if(startCount < START_STOP_COUNT) {
            getManager()->pushEvent<StopServiceEvent>(getServiceId(), _testServiceId);
        } else {
            auto end = std::chrono::steady_clock::now();
            getManager()->pushEvent<QuitEvent>(getServiceId());
            _startServiceRegistration.reset();
            _stopServiceRegistration.reset();
            ICHOR_LOG_INFO(_logger, "dm {} finished in {:L} µs", getManager()->getId(), std::chrono::duration_cast<std::chrono::microseconds>(end-_start).count());
        }
        startCount++;
        return Ichor::StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        getManager()->pushEvent<StartServiceEvent>(getServiceId(), _testServiceId);
        return Ichor::StartBehaviour::SUCCEEDED;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    void addDependencyInstance(ITestService *, IService *isvc) {
        _testServiceId = isvc->getServiceId();
    }

    void removeDependencyInstance(ITestService *, IService *) {
    }

    void handleCompletion(StartServiceEvent const &evt) {
    }

    void handleError(StartServiceEvent const &evt) {
    }

    void handleCompletion(StopServiceEvent const &evt) {
    }

    void handleError(StopServiceEvent const &evt) {
    }

private:
    ILogger *_logger{nullptr};
    uint64_t _testServiceId{0};
    std::chrono::steady_clock::time_point _start{};
    uint64_t startCount{0};
    EventCompletionHandlerRegistration _startServiceRegistration{};
    EventCompletionHandlerRegistration _stopServiceRegistration{};
};