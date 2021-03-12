#include "TestService.h"
#include "OptionalService.h"
#include <ichor/optional_bundles/logging_bundle/LoggerAdmin.h>
#include "MemoryResources.h"
#if defined(NDEBUG)
#include <ichor/optional_bundles/logging_bundle/NullFrameworkLogger.h>
#include <ichor/optional_bundles/logging_bundle/NullLogger.h>

#define FRAMEWORK_LOGGER_TYPE NullFrameworkLogger
#define LOGGER_TYPE NullLogger
#else
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <ichor/optional_bundles/logging_bundle/CoutLogger.h>

#define FRAMEWORK_LOGGER_TYPE CoutFrameworkLogger
#define LOGGER_TYPE CoutLogger
#endif
#include <chrono>
#include <iostream>

using namespace std::string_literals;

std::atomic<uint64_t> idCounter = 0;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::steady_clock::now();

    // Disable
    terminating_resource terminatingResource{};
    std::pmr::set_default_resource(&terminatingResource);
    buffer_resource<1024*256> resourceOne{};
    buffer_resource<1024*256> resourceTwo{};

    DependencyManager dm{&resourceOne, &resourceTwo};
    dm.createServiceManager<FRAMEWORK_LOGGER_TYPE, IFrameworkLogger>({}, 10);
    dm.createServiceManager<LoggerAdmin<LOGGER_TYPE>, ILoggerAdmin>();
    dm.createServiceManager<OptionalService, IOptionalService>();
    dm.createServiceManager<OptionalService, IOptionalService>();
    dm.createServiceManager<TestService>();
    dm.start();
    auto end = std::chrono::steady_clock::now();
#ifndef NDEBUG
    fmt::print("Program ran for {:L} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());
#endif

    return 0;
}