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

class TestBundle : public Bundle {

};

template <auto V>
struct constant {
    constexpr static decltype(V) value = V;
};

int main() {
    Framework fw{{}};
    DependencyManager dm{};
    DependencyInfo dependencies;
    auto mgr = dm.createComponentManager<IFrameworkLogger, SpdlogFrameworkLogger>(dependencies);
    auto *logger = &mgr->getComponent();

    LOG_INFO(logger, "typename: {}", typeName<TestBundle>());
    dm.start();

    return 0;
}