#include <ichor/Assert.h>
#include <cstdio>
#include <cstring>

// Not a Catch2 test, a failing assertion terminates the process. The AssertTests in test/CMakeLists.txt run this and check its output.

[[gnu::noinline]] static void failAlways(int value) {
    ICHOR_ASSERT_ALWAYS(value == 1, "value is {} instead of {}", value, 1);
}

[[gnu::noinline]] static void failHardened(int value) {
    ICHOR_ASSERT(value == 1, "value is {} instead of {}", value, 1);
}

int main(int argc, char **argv) {
    if(argc != 2) {
        std::puts("usage: AssertFailure always|hardened");
        return 1;
    }

    // argc instead of a constant, so that the compiler can't see that the assertion fails
    if(std::strcmp(argv[1], "always") == 0) {
        failAlways(argc);
    } else if(std::strcmp(argv[1], "hardened") == 0) {
        failHardened(argc);
        if constexpr(!DO_INTERNAL_DEBUG && !DO_HARDENING) {
            std::puts("ICHOR_ASSERT isn't checked without hardening");
            return 0;
        }
    }

    std::puts("not terminated");
    return 0;
}
