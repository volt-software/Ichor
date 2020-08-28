#pragma once

#include <framework/DependencyManager.h>
#include <optional_bundles/logging_bundle/Logger.h>
#include "framework/Service.h"
#include "framework/LifecycleManager.h"

using namespace Cppelix;


struct ITestService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class TestService final : public ITestService, public Service {
public:
    ~TestService() final = default;
    bool start() final {
        if(std::any_cast<uint64_t>(getProperties()->operator[]("Iteration")) == 9'999) {
            getManager()->pushEvent<QuitEvent>(getServiceId());
        }
        return true;
    }

    bool stop() final {
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

private:
    ILogger *_logger;
};