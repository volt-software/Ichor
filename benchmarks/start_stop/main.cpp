#include "TestBundle.h"
#include "StartStopBundle.h"
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>

uint64_t StartStopBundle::startCount = 0;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    Framework fw{{}};
    DependencyManager dm{};
    auto logMgr = dm.createComponentManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    spdlog::set_level(spdlog::level::info);
    auto testMgr = dm.createDependencyComponentManager<ITestBundle, TestBundle>(RequiredList<IFrameworkLogger>, OptionalList<>);
    auto startStopMgr = dm.createDependencyComponentManager<IStartStopBundle, StartStopBundle>(RequiredList<IFrameworkLogger, ITestBundle>, OptionalList<>);
    dm.start();

    return 0;
}