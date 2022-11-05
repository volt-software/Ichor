#include "Common.h"
#include <ichor/coroutines/AsyncManualResetEvent.h>

TEST_CASE("AsyncManualResetEventTests") {

    SECTION("default constructor initially not set")
    {
        Ichor::AsyncManualResetEvent event;
        CHECK(!event.is_set());
    }

    SECTION("construct event initially set")
    {
        Ichor::AsyncManualResetEvent event{true};
        CHECK(event.is_set());
    }

    SECTION("set and reset")
    {
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
}