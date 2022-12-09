#include "UsingTcpService.h"
#include "../common/TestMsgRapidJsonSerializer.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerAdmin.h>
#include <ichor/services/network/tcp/TcpHostService.h>
#include <ichor/services/network/ClientAdmin.h>
#include <ichor/services/serialization/ISerializer.h>

#ifdef ICHOR_USE_SPDLOG
#include <ichor/services/logging/SpdlogLogger.h>
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/services/logging/CoutLogger.h>
#define LOGGER_TYPE CoutLogger
#endif

#include <chrono>
#include <iostream>

using namespace std::string_literals;

int main(int argc, char *argv[]) {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::steady_clock::now();
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
#ifdef ICHOR_USE_SPDLOG
    dm.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
    dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>()->setDefaultLogLevel(Ichor::LogLevel::LOG_INFO);
    dm.createServiceManager<TestMsgRapidJsonSerializer, ISerializer<TestMsg>>();
    dm.createServiceManager<TcpHostService, IHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
    dm.createServiceManager<ClientAdmin<TcpConnectionService>, IClientAdmin>();
    dm.createServiceManager<UsingTcpService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
    queue->start(CaptureSigInt);
    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}
