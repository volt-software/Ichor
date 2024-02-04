#pragma once

// gcc 11 wrongfully thinks that there is a null derefence problem when compiling in release mode
#if defined( __GNUC__ )
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
#include <fmt/format.h>
#if defined( __GNUC__ )
#    pragma GCC diagnostic pop
#endif
#include <ichor/Common.h>
#include <ichor/Enums.h>

namespace Ichor {
    class IFrameworkLogger {
    public:
        virtual void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;

        virtual void setLogLevel(LogLevel level) = 0;
        [[nodiscard]] virtual LogLevel getLogLevel() const = 0;
    protected:
        ~IFrameworkLogger() = default;
    };

    template <typename... T>
    auto make_args(T&&... args) {
        return fmt::make_format_args(args...);
    }

#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
#define ICHOR_LOG_TRACE(logger, str, ...) { if(logger != nullptr) logger->trace(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_DEBUG(logger, str, ...) { if(logger != nullptr) logger->debug(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_INFO(logger, str, ...) { if(logger != nullptr) logger->info(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_WARN(logger, str, ...) { if(logger != nullptr) logger->warn(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_ERROR(logger, str, ...) { if(logger != nullptr) logger->error(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, make_args(__VA_ARGS__)); }; static_assert(true, "")

#define ICHOR_LOG_TRACE_ATOMIC(logger, str, ...) { auto *l = logger.load(std::memory_order_acquire); if(l != nullptr) l->trace(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_DEBUG_ATOMIC(logger, str, ...) { auto *l = logger.load(std::memory_order_acquire); if(l != nullptr) l->debug(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_INFO_ATOMIC(logger, str, ...) { auto *l = logger.load(std::memory_order_acquire); if(l != nullptr) l->info(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_WARN_ATOMIC(logger, str, ...) { auto *l = logger.load(std::memory_order_acquire); if(l != nullptr) l->warn(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_ERROR_ATOMIC(logger, str, ...) { auto *l = logger.load(std::memory_order_acquire); if(l != nullptr) l->error(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#else
#define ICHOR_LOG_TRACE(logger, str, ...) { if(logger != nullptr) logger->trace(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_DEBUG(logger, str, ...) { if(logger != nullptr) logger->debug(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_INFO(logger, str, ...) { if(logger != nullptr) logger->info(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_WARN(logger, str, ...) { if(logger != nullptr) logger->warn(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_ERROR(logger, str, ...) { if(logger != nullptr) logger->error(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")

#define ICHOR_LOG_TRACE_ATOMIC(logger, str, ...) { auto *l = logger.load(std::memory_order_acquire); if(l != nullptr) l->trace(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_DEBUG_ATOMIC(logger, str, ...) { auto *l = logger.load(std::memory_order_acquire); if(l != nullptr) l->debug(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_INFO_ATOMIC(logger, str, ...) { auto *l = logger.load(std::memory_order_acquire); if(l != nullptr) l->info(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_WARN_ATOMIC(logger, str, ...) { auto *l = logger.load(std::memory_order_acquire); if(l != nullptr) l->warn(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_ERROR_ATOMIC(logger, str, ...) { auto *l = logger.load(std::memory_order_acquire); if(l != nullptr) l->error(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#endif
}
