#pragma once

#include <chrono>
#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "framework/Bundle.h"
#include "framework/Framework.h"
#include "framework/ComponentLifecycleManager.h"

using namespace Cppelix;


struct IStartStopBundle {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class StartStopBundle : public IStartStopBundle, public Bundle {
public:
    ~StartStopBundle() final = default;
    bool start() final {
        if(startCount == 0) {

            _start = std::chrono::system_clock::now();
        }
        startCount++;
        if(startCount < 1'000'00) {
            _manager->PushEvent<StopBundleEvent>(_testBundleId);
        } else {
            auto end = std::chrono::system_clock::now();
            _manager->PushEvent<QuitEvent>();
            LOG_INFO(_logger, "finished in {:n} µs", std::chrono::duration_cast<std::chrono::microseconds>(end-_start).count());
        }
        return true;
    }

    bool stop() final {
        _manager->PushEvent<StartBundleEvent>(_testBundleId);
        return true;
    }

    void addDependencyInstance(IFrameworkLogger *logger) {
        _logger = logger;
    }

    void removeDependencyInstance(IFrameworkLogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(ITestBundle *bnd) {
        _testBundleId = bnd->get_bundle_id();
    }

    void removeDependencyInstance(ITestBundle *bnd) {
    }

private:
    IFrameworkLogger *_logger{nullptr};
    uint64_t _testBundleId{0};
    std::chrono::system_clock::time_point _start{};
    static uint64_t startCount;
};