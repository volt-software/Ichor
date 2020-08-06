#include "UsingTcpService.h"
#include "TestMsgJsonSerializer.h"
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#include <optional_bundles/network_bundle/tcp/TcpHostService.h>
#include <optional_bundles/network_bundle/ClientAdmin.h>
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
#include <chrono>
#include <iostream>

using namespace std::string_literals;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::system_clock::now();
    DependencyManager dm{};
    dm.createServiceManager<IFrameworkLogger, FRAMEWORK_LOGGER_TYPE>();
    dm.createServiceManager<ILoggerAdmin, LoggerAdmin<LOGGER_TYPE>>(RequiredList<IFrameworkLogger>, OptionalList<>);
    dm.createServiceManager<ISerializationAdmin, SerializationAdmin>(RequiredList<ILogger>, OptionalList<>);
    dm.createServiceManager<ISerializer, TestMsgJsonSerializer>(RequiredList<ILogger, ISerializationAdmin>, OptionalList<>);
    dm.createServiceManager<IHostService, TcpHostService>(RequiredList<ILogger>, OptionalList<>, CppelixProperties{{"Address", "127.0.0.1"s}, {"Port", (uint16_t)8001}});
    dm.createServiceManager<IClientAdmin, ClientAdmin<TcpConnectionService>>();
    dm.createServiceManager<IUsingTcpService, UsingTcpService>(RequiredList<ILogger, ISerializationAdmin, IConnectionService>, OptionalList<>, CppelixProperties{{"Address", "127.0.0.1"s}, {"Port", (uint16_t)8001}});
    dm.start();
    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Program ran for {:n} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}