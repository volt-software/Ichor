#include "OneService.h"
#include "OtherService.h"
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/NullFrameworkLogger.h>
#include <ichor/ichor-mimalloc.h>

// Some compile time logic to instantiate a regular cout logger or to use the spdlog logger, if Ichor has been compiled with it.
#ifdef ICHOR_USE_SPDLOG
#include <ichor/services/logging/SpdlogLogger.h>
#define LOGGER_TYPE SpdlogLogger
#else
#include <ichor/services/logging/CoutLogger.h>
#define LOGGER_TYPE CoutLogger
#endif

#if defined(URING_EXAMPLE)
#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/stl/LinuxUtils.h>

#define QIMPL IOUringQueue
#elif defined(SDEVENT_EXAMPLE)
#include <ichor/event_queues/SdeventQueue.h>

#define QIMPL SdeventQueue
#else
#include <ichor/event_queues/PriorityQueue.h>
#ifdef ORDERED_EXAMPLE
#define QIMPL OrderedPriorityQueue
#else
#define QIMPL PriorityQueue
#endif
#endif

#include <ichor/CommunicationChannel.h>
#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char *argv[]) {
#if ICHOR_EXCEPTIONS_ENABLED
    try {
#endif
        std::locale::global(std::locale("en_US.UTF-8"));
#if ICHOR_EXCEPTIONS_ENABLED
    } catch(std::runtime_error const &e) {
        fmt::println("Couldn't set locale to en_US.UTF-8: {}", e.what());
    }
#endif

#if defined(URING_EXAMPLE)
    auto version = Ichor::v1::kernelVersion();

    if(version < v1::Version{5, 18, 0}) {
        fmt::print("{} kernel version of {} too old to support multithreading. Requires 5.18.0 or newer.\n", argv[0], *version);
        return 0;
    }
#endif

    auto start = std::chrono::steady_clock::now();

    CommunicationChannel channel{};
    auto queueOne = std::make_unique<QIMPL>();
    auto &dmOne = queueOne->createManager();
    auto queueTwo = std::make_unique<QIMPL>();
    auto &dmTwo = queueTwo->createManager();
    channel.addManager(&dmOne);
    channel.addManager(&dmTwo);

    std::thread t1([&] {
#if defined(URING_EXAMPLE)
        queueOne->createEventLoop();
#elif defined(SDEVENT_EXAMPLE)
        auto *loop = queue->createEventLoop();
#endif
#ifdef ICHOR_USE_SPDLOG
        dmOne.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
        dmOne.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
        dmOne.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_INFO)}});
        dmOne.createServiceManager<OneService>();
        queueOne->start(CaptureSigInt);
#if defined(SDEVENT_EXAMPLE)
        sd_event_loop(loop);
#endif
    });

    std::thread t2([&] {
#if defined(URING_EXAMPLE)
        queueTwo->createEventLoop();
#elif defined(SDEVENT_EXAMPLE)
        auto *loop = queue->createEventLoop();
#endif
#ifdef ICHOR_USE_SPDLOG
        dmTwo.createServiceManager<SpdlogSharedService, ISpdlogSharedService>();
#endif
        dmTwo.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
        dmTwo.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_INFO)}});
        dmTwo.createServiceManager<OtherService>();
        queueTwo->start(CaptureSigInt);
#if defined(SDEVENT_EXAMPLE)
        sd_event_loop(loop);
#endif
    });

    t1.join();
    t2.join();

    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}
