#include "Common.h"
#include <ichor/coroutines/AsyncAutoResetEvent.h>

TEST_CASE("AsyncAutoResetEventTests") {

    SECTION("default constructor")
    {
        Ichor::AsyncAutoResetEvent event;
    }
}