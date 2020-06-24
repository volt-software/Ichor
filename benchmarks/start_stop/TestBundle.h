#pragma once

#include <framework/DependencyManager.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "framework/Bundle.h"
#include "framework/Framework.h"
#include "framework/ComponentLifecycleManager.h"

using namespace Cppelix;


struct ITestBundle : virtual public IBundle {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class TestBundle : public ITestBundle, public Bundle {
public:
    ~TestBundle() final = default;
    bool start() final {
        return true;
    }

    bool stop() final {
        return true;
    }

    void addDependencyInstance(IFrameworkLogger *logger) {
        _logger = logger;
    }

    void removeDependencyInstance(IFrameworkLogger *logger) {
        _logger = nullptr;
    }

private:
    IFrameworkLogger *_logger;
};