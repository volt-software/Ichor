#pragma once

#include <thread>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <ichor/ichor-mimalloc.h>

using namespace Ichor;
using namespace Ichor::v1;

void waitForRunning(DependencyManager &dm) {
    while(!dm.isRunning()) {
        std::this_thread::sleep_for(1ms);
    }
}

void runForOrQueueEmpty(DependencyManager &dm) {
#ifdef ICHOR_MUSL
    // likely that is run on a slower device or in emulation, requiring more time.
    std::this_thread::sleep_for(50ms);
    dm.runForOrQueueEmpty(1'000ms);
#else
    dm.runForOrQueueEmpty();
#endif
}

class ExceptionMatcher final : public Catch::Matchers::MatcherBase<std::exception> {
public:
    ExceptionMatcher() = default;
    ~ExceptionMatcher() final = default;

    bool match(std::exception const& ex) const final {
        return true;
    }

    std::string describe() const final {
        return "match exception exactly"; // by not providing custom logic to REQUIRE_THROWS_MATCHES :)
    }
};

#define NAMEOF(x) #x
