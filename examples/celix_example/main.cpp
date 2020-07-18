#include "CelixService.h"
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#include <optional_bundles/logging_bundle/SpdlogLogger.h>
#include <framework/CommunicationChannel.h>
#include <chrono>
#include <iostream>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::system_clock::now();

    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    auto logAdminMgr = dm.createServiceManager<ILoggerAdmin, LoggerAdmin<SpdlogLogger>>();
    auto oneService = dm.createServiceManager<ICelixService, CelixService>(RequiredList<ILogger>, OptionalList<>);
    dm.start();

    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Program ran for {:n} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}