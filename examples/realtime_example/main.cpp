#include "TestService.h"
#include "OptionalService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/logging/LoggerAdmin.h>
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

std::atomic<uint64_t> idCounter = 0;
std::string_view progName;

void* run_example(void*) {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
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
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
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
    std::locale::global(std::locale("en_US.UTF-8"));
    std::ios::sync_with_stdio(false);
    progName = argv[0];

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    //TODO check for elevated permissions
#else

    uid_t uid = getuid();
    uid_t euid = geteuid();

    if (uid !=0 || uid!=euid) {
        throw std::runtime_error("No permissions to set realtime scheduling. Consider running under sudo/root.");
    }
#endif

    GlobalRealtimeSettings settings{};

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    //TODO realtime settings
    run_example();
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
        throw std::runtime_error("No permissions to set realtime scheduling. Consider running under sudo/root.");
    }

    if(ret == 0) {
        pthread_join(thread, nullptr);
    }
#endif

    return 0;
}
