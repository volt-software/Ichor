#include "TestService.h"
#include "TestMsgJsonSerializer.h"
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#include <optional_bundles/logging_bundle/SpdlogLogger.h>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    spdlog::set_level(spdlog::level::info);
    auto logAdminMgr = dm.createServiceManager<ILoggerAdmin, LoggerAdmin<SpdlogLogger>>();
    auto serAdmin = dm.createDependencyServiceManager<ISerializationAdmin, SerializationAdmin>(RequiredList<ILogger>, OptionalList<>);
    auto testMsgSerializer = dm.createDependencyServiceManager<ISerializer, TestMsgJsonSerializer>(RequiredList<ILogger, ISerializationAdmin>, OptionalList<>);
    auto testMgr = dm.createDependencyServiceManager<ITestService, TestService>(RequiredList<ILogger, ISerializationAdmin>, OptionalList<>);
    dm.start();

    return 0;
}