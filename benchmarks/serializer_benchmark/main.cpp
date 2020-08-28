#include "TestService.h"
#include "TestMsgJsonSerializer.h"
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#ifdef USE_SPDLOG
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <optional_bundles/logging_bundle/SpdlogLogger.h>

#define FRAMEWORK_LOGGER_TYPE SpdlogFrameworkLogger
#define LOGGER_TYPE SpdlogLogger
#define LOGGER_SHARED_TYPE ,ISpdlogSharedService
#else
#include <optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <optional_bundles/logging_bundle/CoutLogger.h>

#define FRAMEWORK_LOGGER_TYPE CoutFrameworkLogger
#define LOGGER_TYPE CoutLogger
#define LOGGER_SHARED_TYPE
#endif

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, FRAMEWORK_LOGGER_TYPE>();
    logMgr->setLogLevel(LogLevel::INFO);
#ifdef USE_SPDLOG
    dm.createServiceManager<ISpdlogSharedService, SpdlogSharedService>();
#endif
    dm.createServiceManager<ILoggerAdmin, LoggerAdmin<LOGGER_TYPE LOGGER_SHARED_TYPE>>(RequiredList<IFrameworkLogger>);
    dm.createServiceManager<ISerializationAdmin, SerializationAdmin>(RequiredList<ILogger>, OptionalList<>);
    dm.createServiceManager<ISerializer, TestMsgJsonSerializer>(RequiredList<ILogger, ISerializationAdmin>, OptionalList<>);
    dm.createServiceManager<ITestService, TestService>(RequiredList<ILogger, ISerializationAdmin>, OptionalList<>);
    dm.start();

    return 0;
}