#pragma once

#include <cppelix/DependencyManager.h>
#include <cppelix/optional_bundles/logging_bundle/Logger.h>
#include <cppelix/Service.h>
#include <cppelix/LifecycleManager.h>

using namespace Cppelix;


struct ITestService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class TestService final : public ITestService, public Service {
public:
    TestService(DependencyRegister &reg, CppelixProperties props) : Service(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~TestService() final = default;
    bool start() final {
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