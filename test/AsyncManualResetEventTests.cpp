#include "Common.h"
#include <ichor/coroutines/AsyncManualResetEvent.h>

TEST_CASE("AsyncManualResetEventTests: default constructor initially not set") {
    Ichor::AsyncManualResetEvent event;
    CHECK(!event.is_set());
}

TEST_CASE("AsyncManualResetEventTests: construct event initially set") {
    Ichor::AsyncManualResetEvent event{true};
    CHECK(event.is_set());
}

TEST_CASE("AsyncManualResetEventTests: set and reset") {
    Ichor::AsyncManualResetEvent event;
    CHECK(!event.is_set());
    event.set();
    CHECK(event.is_set());
    event.set();
    CHECK(event.is_set());
    event.reset();
    CHECK(!event.is_set());
    event.reset();
    CHECK(!event.is_set());
    event.set();
    CHECK(event.is_set());
}
