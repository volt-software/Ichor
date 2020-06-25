#include "TestService.h"
#include "TestMsgJsonSerializer.h"
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    Framework fw{{}};
    DependencyManager dm{};
    auto logMgr = dm.createComponentManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    spdlog::set_level(spdlog::level::info);
    auto serAdmin = dm.createDependencyComponentManager<ISerializationAdmin, SerializationAdmin>(RequiredList<IFrameworkLogger>, OptionalList<>);
    auto testMsgSerializer = dm.createDependencyComponentManager<ISerializer, TestMsgJsonSerializer>(RequiredList<IFrameworkLogger, ISerializationAdmin>, OptionalList<>);
    auto testMgr = dm.createDependencyComponentManager<ITestService, TestService>(RequiredList<IFrameworkLogger, ISerializationAdmin>, OptionalList<>);
    dm.start();

    return 0;
}