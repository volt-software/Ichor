#pragma once

#include <thread>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>

using namespace Ichor;

void waitForRunning(DependencyManager &dm) {
    while(!dm.isRunning()) {
        std::this_thread::sleep_for(1ms);
    }
}

class ExceptionMatcher final : public Catch::Matchers::MatcherBase<std::exception> {
    std::string m_message;
public:

    ExceptionMatcher() {}

    bool match(std::exception const& ex) const final {
        return true;
    }

    std::string describe() const final {
        return "match exception exactly"; // by not providing custom logic to REQUIRE_THROWS_MATCHES :)
    }
};
