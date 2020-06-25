#pragma once

#include <chrono>
#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "framework/Service.h"
#include "framework/Framework.h"
#include "framework/ComponentLifecycleManager.h"

using namespace Cppelix;


struct IStartStopService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class StartStopService : public IStartStopService, public Service {
public:
    ~StartStopService() final = default;
    bool start() final {
        if(startCount == 0) {

            _start = std::chrono::system_clock::now();
        }
        startCount++;
        if(startCount < 1'000'00) {
            _manager->PushEvent<StopServiceEvent>(_testServiceId);
        } else {
            auto end = std::chrono::system_clock::now();
            _manager->PushEvent<QuitEvent>();
            LOG_INFO(_logger, "finished in {:n} µs", std::chrono::duration_cast<std::chrono::microseconds>(end-_start).count());
        }
        return true;
    }

    bool stop() final {
        _manager->PushEvent<StartServiceEvent>(_testServiceId);
        return true;
    }

    void addDependencyInstance(IFrameworkLogger *logger) {
        _logger = logger;
    }

    void removeDependencyInstance(IFrameworkLogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(ITestService *bnd) {
        _testServiceId = bnd->get_service_id();
    }

    void removeDependencyInstance(ITestService *bnd) {
    }

private:
    IFrameworkLogger *_logger{nullptr};
    uint64_t _testServiceId{0};
    std::chrono::system_clock::time_point _start{};
    static uint64_t startCount;
};