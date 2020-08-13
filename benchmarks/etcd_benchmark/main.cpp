#include "UsingEtcdService.h"
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#include <optional_bundles/etcd_bundle/EtcdService.h>
#ifdef USE_SPDLOG
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <optional_bundles/logging_bundle/SpdlogLogger.h>

#define FRAMEWORK_LOGGER_TYPE SpdlogFrameworkLogger
#define LOGGER_TYPE SpdlogLogger
#else
#include <optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <optional_bundles/logging_bundle/CoutLogger.h>

#define FRAMEWORK_LOGGER_TYPE CoutFrameworkLogger
#define LOGGER_TYPE CoutLogger
#endif
#include <string>

int main() {
    using namespace std::string_literals;
    std::locale::global(std::locale("en_US.UTF-8"));

    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, FRAMEWORK_LOGGER_TYPE>();
    logMgr->setLogLevel(LogLevel::INFO);
    dm.createServiceManager<ILoggerAdmin, LoggerAdmin<LOGGER_TYPE>>();
    dm.createServiceManager<IEtcdService, EtcdService>(RequiredList<ILogger>, OptionalList<>, CppelixProperties{{"EtcdAddress", "localhost:2379"s}});
    dm.createServiceManager<IUsingEtcdService, UsingEtcdService>(RequiredList<ILogger, IEtcdService>, OptionalList<>);
    dm.start();

    return 0;
}