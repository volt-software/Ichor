#include "TestService.h"
#include "TestMsgJsonSerializer.h"
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#include <optional_bundles/logging_bundle/SpdlogLogger.h>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    logMgr->setLogLevel(LogLevel::INFO);
    auto logAdminMgr = dm.createServiceManager<ILoggerAdmin, LoggerAdmin<SpdlogLogger>>();
    auto serAdmin = dm.createServiceManager<ISerializationAdmin, SerializationAdmin>(RequiredList<ILogger>, OptionalList<>);
    auto testMsgSerializer = dm.createServiceManager<ISerializer, TestMsgJsonSerializer>(RequiredList<ILogger, ISerializationAdmin>, OptionalList<>);
    auto testMgr = dm.createServiceManager<ITestService, TestService>(RequiredList<ILogger, ISerializationAdmin>, OptionalList<>);
    dm.start();

    return 0;
}