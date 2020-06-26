#pragma once

#include <chrono>
#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "framework/Service.h"
#include "framework/Framework.h"
#include "framework/ServiceLifecycleManager.h"

using namespace Cppelix;


struct IStartStopService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class StartStopService : public IStartStopService, public Service {
public:
    ~StartStopService() final = default;
    bool start() final {
        if(startCount == 0) {
            _manager->registerCompletionCallback<StartServiceEvent>(getServiceId(), this);
            _manager->registerCompletionCallback<StopServiceEvent>(getServiceId(), this);

            _start = std::chrono::system_clock::now();
            _manager->PushEvent<StopServiceEvent>(getServiceId(), _testServiceId);
        } else if(startCount < 1'000'000) {
            _manager->PushEvent<StopServiceEvent>(getServiceId(), _testServiceId);
        } else {
            auto end = std::chrono::system_clock::now();
            _manager->PushEvent<QuitEvent>(getServiceId());
            LOG_INFO(_logger, "finished in {:n} µs", std::chrono::duration_cast<std::chrono::microseconds>(end-_start).count());
        }
        startCount++;
        return true;
    }

    bool stop() final {
        _manager->PushEvent<StartServiceEvent>(getServiceId(), _testServiceId);
        return true;
    }

    void addDependencyInstance(IFrameworkLogger *logger) {
        _logger = logger;
    }

    void removeDependencyInstance(IFrameworkLogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(ITestService *bnd) {
        _testServiceId = bnd->getServiceId();
    }

    void removeDependencyInstance(ITestService *bnd) {
    }

    void handleCompletion(StartServiceEvent const * const evt) {
    }

    void handleCompletion(StopServiceEvent const * const evt) {
    }

private:
    IFrameworkLogger *_logger{nullptr};
    uint64_t _testServiceId{0};
    std::chrono::system_clock::time_point _start{};
    static uint64_t startCount;
};