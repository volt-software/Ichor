#include "TestService.h"
#include "TestMsgJsonSerializer.h"
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    spdlog::set_level(spdlog::level::info);
    auto serAdmin = dm.createDependencyServiceManager<ISerializationAdmin, SerializationAdmin>(RequiredList<IFrameworkLogger>, OptionalList<>);
    auto testMsgSerializer = dm.createDependencyServiceManager<ISerializer, TestMsgJsonSerializer>(RequiredList<IFrameworkLogger, ISerializationAdmin>, OptionalList<>);
    auto testMgr = dm.createDependencyServiceManager<ITestService, TestService>(RequiredList<IFrameworkLogger, ISerializationAdmin>, OptionalList<>);
    dm.start();

    return 0;
}