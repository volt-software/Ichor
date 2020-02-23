//
// Created by oipo on 13-02-20.
//

#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include "test.h"
#include "../include/framework/Bundle.h"
#include "../include/framework/Framework.h"
#include "framework/ComponentLifecycleManager.h"

using namespace Cppelix;

struct ITestBundle {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class TestBundle : public ITestBundle, public Bundle {

public:
    ~TestBundle() final = default;
    bool start() final {
        LOG_INFO(_logger, "TestBundle started with dependency");
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "TestBundle stopped with dependency");
        return true;
    }

    void addDependencyInstance(IFrameworkLogger *logger) {
        _logger = logger;
        LOG_INFO(_logger, "Inserted logger");
    }

private:
    IFrameworkLogger *_logger;
};

int main() {
    Framework fw{{}};
    DependencyManager dm{};
    auto logMgr = dm.createComponentManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    auto testMgr = dm.createDependencyComponentManager<ITestBundle, TestBundle, IFrameworkLogger>();
    auto *logger = &logMgr->getComponent();

    LOG_INFO(logger, "typename: {}", typeName<TestBundle>());
    dm.start();

    return 0;
}