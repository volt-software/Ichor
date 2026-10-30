#include <ichor/Assert.h>
#include <ichor/stl/StringUtils.h>
#include <fmt/base.h>
#include <cstdio>
#include <exception>
#ifdef ICHOR_HAVE_STD_STACKTRACE
#include <stacktrace>
#include <string>
#endif

void Ichor::v1::Detail::assertionFailed(char const *condition, char const *file, int line, std::string_view message) noexcept {
    if(file != nullptr) {
        fmt::println(stderr, "[{}:{}] Assertion `{}` failed: {}", Ichor::v1::basename(file), line, condition, message);
    } else {
        fmt::println(stderr, "Assertion failed: {}", message);
    }

#ifdef ICHOR_HAVE_STD_STACKTRACE
    // skip this function itself
    fmt::println(stderr, "{}", std::to_string(std::stacktrace::current(1)));
#elif defined(ICHOR_USE_BACKWARD)
    fmt::println(stderr, "(no std::stacktrace support, backward-cpp prints the stack trace when terminating)");
#else
    fmt::println(stderr, "(no stack trace, the compiler or standard library doesn't support std::stacktrace)");
#endif

    std::fflush(nullptr);
    std::terminate();
}
