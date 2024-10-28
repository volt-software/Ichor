#pragma once

#if defined(__has_cpp_attribute) && __has_cpp_attribute(assume)
#define ICHOR_ASSUME(...) [[assume(__VA_ARGS__)]];
#else
#define ICHOR_ASSUME(...) static_assert(true, "")
#endif

#if __has_cpp_attribute(clang::lifetimebound)
#define ICHOR_LIFETIME_BOUND [[clang::lifetimebound]]
#else
#define ICHOR_LIFETIME_BOUND
#endif

#if __has_cpp_attribute(clang::coro_lifetimebound)
#define ICHOR_CORO_LIFETIME_BOUND [[clang::coro_lifetimebound]]
#else
#define ICHOR_CORO_LIFETIME_BOUND
#endif

#if __has_cpp_attribute(clang::coro_disable_lifetimebound)
#define ICHOR_CORO_DISABLE_LIFETIME_BOUND [[clang::coro_disable_lifetimebound]]
#else
#define ICHOR_CORO_DISABLE_LIFETIME_BOUND
#endif

#if __has_cpp_attribute(clang::coro_await_elidable)
#define ICHOR_CORO_AWAIT_ELIDABLE [[clang::coro_await_elidable]]
#else
#define ICHOR_CORO_AWAIT_ELIDABLE
#endif

#if __has_cpp_attribute(clang::coro_return_type)
#define ICHOR_CORO_RETURN_TYPE [[clang::coro_return_type]]
#else
#define ICHOR_CORO_RETURN_TYPE
#endif

#if __has_cpp_attribute(clang::coro_wrapper)
#define ICHOR_CORO_WRAPPER [[clang::coro_wrapper]]
#else
#define ICHOR_CORO_WRAPPER
#endif

#if __cplusplus >= 202100L
#define ICHOR_CXX23_CONSTEXPR constexpr
#else
#define ICHOR_CXX23_CONSTEXPR
#endif
