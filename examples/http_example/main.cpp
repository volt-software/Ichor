#include "UsingHttpService.h"
#include "../common/TestMsgJsonSerializer.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/optional_bundles/logging_bundle/LoggerAdmin.h>
#include <ichor/optional_bundles/network_bundle/http/HttpHostService.h>
#include <ichor/optional_bundles/network_bundle/http/HttpConnectionService.h>
#include <ichor/optional_bundles/network_bundle/http/HttpContextService.h>
#include <ichor/optional_bundles/network_bundle/ClientAdmin.h>
#include <ichor/optional_bundles/serialization_bundle/SerializationAdmin.h>
#ifdef ICHOR_USE_SPDLOG
#include <ichor/optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <ichor/optional_bundles/logging_bundle/SpdlogLogger.h>

#define FRAMEWORK_LOGGER_TYPE SpdlogFrameworkLogger
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <ichor/optional_bundles/logging_bundle/CoutLogger.h>

#define FRAMEWORK_LOGGER_TYPE CoutFrameworkLogger
#define LOGGER_TYPE CoutLogger
#endif
#include <chrono>
#include <iostream>

using namespace std::string_literals;
using namespace Ichor;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::steady_clock::now();
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
    dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
#ifdef ICHOR_USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
    dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
    dm.createServiceManager<SerializationAdmin, ISerializationAdmin>();
    dm.createServiceManager<TestMsgJsonSerializer, ISerializer>();
    dm.createServiceManager<HttpContextService, IHttpContextService>();
    dm.createServiceManager<HttpHostService, IHttpService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(8001)}});
    dm.createServiceManager<ClientAdmin<HttpConnectionService, IHttpConnectionService>, IClientAdmin>();
    dm.createServiceManager<UsingHttpService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(8001)}});
    queue->start(CaptureSigInt);
    auto end = std::chrono::steady_clock::now();
    fmt::print("Program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}