#pragma once

#include <chrono>
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;

class StartStopService final : public Service {
public:
    StartStopService(DependencyRegister &reg, IchorProperties props) : Service(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ITestService>(this, true);
    }
    ~StartStopService() final = default;
    bool start() final {
        if(startCount == 0) {
            _startServiceRegistration = getManager()->registerEventCompletionCallbacks<StartServiceEvent>(getServiceId(), this);
            _stopServiceRegistration = getManager()->registerEventCompletionCallbacks<StopServiceEvent>(getServiceId(), this);

            _start = std::chrono::system_clock::now();
            getManager()->pushEvent<StopServiceEvent>(getServiceId(), _testServiceId);
        } else if(startCount < 1'000'000) {
            getManager()->pushEvent<StopServiceEvent>(getServiceId(), _testServiceId);
        } else {
            auto end = std::chrono::system_clock::now();
            getManager()->pushEvent<QuitEvent>(getServiceId());
            LOG_INFO(_logger, "finished in {:L} µs", std::chrono::duration_cast<std::chrono::microseconds>(end-_start).count());
        }
        startCount++;
        return true;
    }

    bool stop() final {
        getManager()->pushEvent<StartServiceEvent>(getServiceId(), _testServiceId);
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(ITestService *bnd) {
        _testServiceId = bnd->getServiceId();
    }

    void removeDependencyInstance(ITestService *bnd) {
    }

    void handleCompletion(StartServiceEvent const * const evt) {
    }

    void handleError(StartServiceEvent const * const evt) {
    }

    void handleCompletion(StopServiceEvent const * const evt) {
    }

    void handleError(StopServiceEvent const * const evt) {
    }

private:
    ILogger *_logger{nullptr};
    uint64_t _testServiceId{0};
    std::chrono::system_clock::time_point _start{};
    static uint64_t startCount;
    std::unique_ptr<EventCompletionHandlerRegistration> _startServiceRegistration;
    std::unique_ptr<EventCompletionHandlerRegistration> _stopServiceRegistration;
};