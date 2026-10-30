#pragma once

#include <ichor/Defines.h>
#include <fmt/format.h>
#include <string_view>

namespace Ichor::v1::Detail {
    /// Prints the failed assertion, followed by a stack trace when the compiler supports std::stacktrace, and terminates.
    /// Use ICHOR_ASSERT or ICHOR_ASSERT_ALWAYS instead of calling this directly.
    [[noreturn]] void assertionFailed(char const *condition, char const *file, int line, std::string_view message) noexcept;
}

#ifdef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
#define ICHOR_DETAIL_ASSERTION_FAILED(cond, ...) ::Ichor::v1::Detail::assertionFailed(nullptr, nullptr, 0, ::fmt::format(__VA_ARGS__))
#else
#define ICHOR_DETAIL_ASSERTION_FAILED(cond, ...) ::Ichor::v1::Detail::assertionFailed(#cond, __FILE__, __LINE__, ::fmt::format(__VA_ARGS__))
#endif

/// Terminates the program when cond is false, after printing the message, the condition, where it failed and a stack trace (when the
/// compiler supports std::stacktrace). Only checked when DO_INTERNAL_DEBUG or DO_HARDENING, cond isn't evaluated otherwise.
/// The message is a fmt format string followed by its arguments, which are only evaluated when the assertion fails. Say what went wrong
/// and, when it isn't obvious, why it isn't allowed, e.g. ICHOR_ASSERT(isStarted(), "factory for {} not started, timers need its queue", id);
/// Like with any macro, a condition with a comma outside of parentheses (e.g. Foo<A, B>::bar()) needs to be put in parentheses itself.
#define ICHOR_ASSERT(cond, ...) \
    do { \
        if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) { \
            if(!(cond)) [[unlikely]] { \
                ICHOR_DETAIL_ASSERTION_FAILED(cond, __VA_ARGS__); \
            } \
        } \
    } while(false)

/// Like ICHOR_ASSERT, but always checked. For checks that guard against undefined behaviour or that are cheap compared to what they check,
/// and for checks inside code that is already only compiled in for a specific kind of debugging.
#define ICHOR_ASSERT_ALWAYS(cond, ...) \
    do { \
        if(!(cond)) [[unlikely]] { \
            ICHOR_DETAIL_ASSERTION_FAILED(cond, __VA_ARGS__); \
        } \
    } while(false)
