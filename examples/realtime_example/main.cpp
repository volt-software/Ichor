#include "TestService.h"
#include "OptionalService.h"
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/NullFrameworkLogger.h>
#include <ichor/ichor-mimalloc.h>
#include "GlobalRealtimeSettings.h"

#if defined(NDEBUG)
#include <ichor/services/logging/NullLogger.h>
#define LOGGER_TYPE NullLogger
#else
#include <ichor/services/logging/CoutLogger.h>
#define LOGGER_TYPE CoutLogger
#endif

#include <chrono>
#include <stdexcept>

using namespace std::string_literals;

std::string_view progName;

void* run_example(void*) {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32) || defined(__APPLE__)) && !defined(__CYGWIN__)
    //TODO realtime settings
#else
    cpu_set_t lock_to_core_set;
    CPU_ZERO(&lock_to_core_set);
    CPU_SET(1, &lock_to_core_set);
    sched_setaffinity(0, sizeof(cpu_set_t), &lock_to_core_set);
#endif

#ifndef NDEBUG
    auto start = std::chrono::steady_clock::now();
#endif

    {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
        dm.createServiceManager<LoggerFactory<LOGGER_TYPE>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_INFO)}});
        dm.createServiceManager<OptionalService, IOptionalService>();
        dm.createServiceManager<OptionalService, IOptionalService>();
        dm.createServiceManager<TestService>();
        queue->start(CaptureSigInt);
    }
#ifndef NDEBUG
    auto end = std::chrono::steady_clock::now();
    fmt::print("{} ran for {:L} µs\n", progName, std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());
#endif

    return nullptr;
}

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
    progName = argv[0];

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32) || defined(__APPLE__)) && !defined(__CYGWIN__)
    //TODO check for elevated permissions
#else

    uid_t uid = getuid();
    uid_t euid = geteuid();

    if (uid !=0 || uid!=euid) {
        fmt::println("This example requires running under sudo/root.");
        std::terminate();
    }
#endif

    GlobalRealtimeSettings settings{};

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32) || defined(__APPLE__)) && !defined(__CYGWIN__)
    //TODO realtime settings
    run_example(nullptr);
    int ret = 0;
#else
    // create a thread with realtime priority to run the program on
    pthread_t thread{};
    sched_param param{};
    pthread_attr_t attr{};
    pthread_attr_init (&attr);
    pthread_attr_setinheritsched (&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy (&attr, SCHED_FIFO);
    param.sched_priority = 20;
    pthread_attr_setschedparam (&attr, &param);
    auto ret = pthread_create (&thread, &attr, run_example, nullptr);

    if(ret == EPERM) {
        fmt::println("No permissions to set realtime thread scheduling. Consider running under sudo/root.");
        std::terminate();
    }

    if(ret == 0) {
        pthread_join(thread, nullptr);
    }
#endif

    return ret == 0 ? 0 : 1;
}
