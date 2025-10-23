#pragma once

#if defined(ICHOR_ENABLE_INTERNAL_DEBUGGING) || defined(ICHOR_ENABLE_INTERNAL_COROUTINE_DEBUGGING) || defined(ICHOR_ENABLE_INTERNAL_IO_DEBUGGING) || defined(ICHOR_ENABLE_INTERNAL_STL_DEBUGGING)
#include <chrono>
#include <fmt/base.h>
#include <thread>
#include <fmt/std.h>
#include <ichor/stl/StringUtils.h>
#endif

#ifdef ICHOR_ENABLE_INTERNAL_DEBUGGING
static constexpr bool DO_INTERNAL_DEBUG = true;

#define INTERNAL_DEBUG(...)      \
    if constexpr(DO_INTERNAL_DEBUG) { \
        std::thread::id this_id = std::this_thread::get_id();                         \
        fmt::print("[time:{:L}] [thread:{}] ", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), this_id);    \
        const char *base = Ichor::v1::basename(__FILE__); \
        fmt::print("[{}:{}] ", base, __LINE__);    \
        fmt::println(__VA_ARGS__);    \
    }                                 \
static_assert(true, "")
#else
static constexpr bool DO_INTERNAL_DEBUG = false;

#define INTERNAL_DEBUG(...) static_assert(true, "")
#endif

#ifdef ICHOR_ENABLE_INTERNAL_COROUTINE_DEBUGGING
static constexpr bool DO_INTERNAL_COROUTINE_DEBUG = true;

#define INTERNAL_COROUTINE_DEBUG(...)      \
    if constexpr(DO_INTERNAL_COROUTINE_DEBUG) { \
        std::thread::id this_id = std::this_thread::get_id();                         \
        fmt::print("[{:L}] [{}] ", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), this_id);    \
        const char *base = Ichor::v1::basename(__FILE__); \
        fmt::print("[{}:{}] ", base, __LINE__);    \
        fmt::println(__VA_ARGS__);    \
    }                                 \
static_assert(true, "")
#else
static constexpr bool DO_INTERNAL_COROUTINE_DEBUG = false;

#define INTERNAL_COROUTINE_DEBUG(...) static_assert(true, "")
#endif

#ifdef ICHOR_ENABLE_INTERNAL_IO_DEBUGGING
static constexpr bool DO_INTERNAL_IO_DEBUG = true;

#define INTERNAL_IO_DEBUG(...)      \
    if constexpr(DO_INTERNAL_IO_DEBUG) { \
        std::thread::id this_id = std::this_thread::get_id();                         \
        fmt::print("[{:L}] [{}] ", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), this_id);    \
        const char *base = Ichor::v1::basename(__FILE__); \
        fmt::print("[{}:{}] ", base, __LINE__);    \
        fmt::println(__VA_ARGS__);    \
    }                                 \
static_assert(true, "")
#else
static constexpr bool DO_INTERNAL_IO_DEBUG = false;

#define INTERNAL_IO_DEBUG(...) static_assert(true, "")
#endif

#ifdef ICHOR_ENABLE_INTERNAL_STL_DEBUGGING
static constexpr bool DO_INTERNAL_STL_DEBUG = true;

#define INTERNAL_STL_DEBUG(...)      \
    if constexpr(DO_INTERNAL_STL_DEBUG) { \
        std::thread::id this_id = std::this_thread::get_id();                         \
        fmt::print("[{:L}] [{}] ", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), this_id);    \
        const char *base = Ichor::v1::basename(__FILE__); \
        fmt::print("[{}:{}] ", base, __LINE__);    \
        fmt::println(__VA_ARGS__);    \
    }                                 \
static_assert(true, "")
#else
static constexpr bool DO_INTERNAL_STL_DEBUG = false;

#define INTERNAL_STL_DEBUG(...) static_assert(true, "")
#endif

#ifdef ICHOR_USE_HARDENING
static constexpr bool DO_HARDENING = true;
#else
static constexpr bool DO_HARDENING = false;
#endif

#if defined(__cpp_contracts)
#define ICHOR_CONTRACT_ASSERT contract_assert
#else
#define ICHOR_CONTRACT_ASSERT(expr) if constexpr (false) {  (void)!!(expr); }
#endif

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define ICHOR_EXCEPTIONS_ENABLED 1
#else
#define ICHOR_EXCEPTIONS_ENABLED 0
#endif

#ifdef ICHOR_HAVE_CONSTEXPR_VECTOR
#define ICHOR_CONSTEXPR_VECTOR constexpr
#define ICHOR_CONSTINIT_VECTOR constinit
#else
#define ICHOR_CONSTEXPR_VECTOR
#define ICHOR_CONSTINIT_VECTOR
#endif
