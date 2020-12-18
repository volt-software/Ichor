#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;

class TestService final : public Service {
public:
    TestService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~TestService() final = default;
    bool start() final {
        auto iteration = std::any_cast<uint64_t>(getProperties()->operator[]("Iteration"));
        if(iteration == 9'999) {
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
    ILogger *_logger{nullptr};
};