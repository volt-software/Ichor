#pragma once

#include <fmt/base.h>
#include <ichor/Defines.h>
#include <ichor/CoreTypes.h>
#include <ichor/Enums.h>

#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
#include <ichor/stl/StringUtils.h>
#endif

namespace Ichor {
    class IFrameworkLogger {
    public:
        virtual void trace(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS = 0;
        virtual void debug(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS = 0;
        virtual void info(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS = 0;
        virtual void warn(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS = 0;
        virtual void error(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS = 0;

        virtual void setLogLevel(LogLevel level) noexcept = 0;
        [[nodiscard]] virtual LogLevel getLogLevel() const noexcept = 0;
        [[nodiscard]] virtual ServiceIdType getFrameworkServiceId() const noexcept = 0;
    protected:
        ~IFrameworkLogger() = default;
    };

    template <typename... T>
    auto make_args(T&&... args) {
        return fmt::make_format_args(args...);
    }

#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
#define ICHOR_LOG_TRACE(logger, str, ...) { if(logger != nullptr && logger->getLogLevel() <= Ichor::LogLevel::LOG_TRACE) logger->trace(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, Ichor::make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_DEBUG(logger, str, ...) { if(logger != nullptr && logger->getLogLevel() <= Ichor::LogLevel::LOG_DEBUG) logger->debug(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, Ichor::make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_INFO(logger, str, ...) { if(logger != nullptr && logger->getLogLevel() <= Ichor::LogLevel::LOG_INFO) logger->info(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, Ichor::make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_WARN(logger, str, ...) { if(logger != nullptr && logger->getLogLevel() <= Ichor::LogLevel::LOG_WARN) logger->warn(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, Ichor::make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_ERROR(logger, str, ...) { if(logger != nullptr) logger->error(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, Ichor::make_args(__VA_ARGS__)); }; static_assert(true, "")

#define ICHOR_EMERGENCY_LOG1(logger, str) { if(logger != nullptr) { logger->error(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, Ichor::make_args()); } const char *base = Ichor::v1::basename(__FILE__); fmt::print("[{}:{}] ", base, __LINE__); fmt::println(str); }; static_assert(true, "")
#define ICHOR_EMERGENCY_LOG2(logger, str, ...) { if(logger != nullptr) { logger->error(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, Ichor::make_args(__VA_ARGS__)); } const char *base = Ichor::v1::basename(__FILE__); fmt::print("[{}:{}] ", base, __LINE__); fmt::println(str, __VA_ARGS__); }; static_assert(true, "")
#define ICHOR_EMERGENCY_NO_LOGGER_LOG1(str) { const char *base = Ichor::v1::basename(__FILE__); fmt::print("[{}:{}] ", base, __LINE__); fmt::println(str); }; static_assert(true, "")
#define ICHOR_EMERGENCY_NO_LOGGER_LOG2(str, ...) { const char *base = Ichor::v1::basename(__FILE__); fmt::print("[{}:{}] ", base, __LINE__); fmt::println(str, __VA_ARGS__); }; static_assert(true, "")
#else
#define ICHOR_LOG_TRACE(logger, str, ...) { if(logger != nullptr && logger->getLogLevel() <= Ichor::LogLevel::LOG_TRACE) logger->trace(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_DEBUG(logger, str, ...) { if(logger != nullptr && logger->getLogLevel() <= Ichor::LogLevel::LOG_DEBUG) logger->debug(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_INFO(logger, str, ...) { if(logger != nullptr && logger->getLogLevel() <= Ichor::LogLevel::LOG_INFO) logger->info(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_WARN(logger, str, ...) { if(logger != nullptr && logger->getLogLevel() <= Ichor::LogLevel::LOG_WARN) logger->warn(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")
#define ICHOR_LOG_ERROR(logger, str, ...) { if(logger != nullptr) logger->error(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); }; static_assert(true, "")

#define ICHOR_EMERGENCY_LOG1(logger, str) { if(logger != nullptr) { logger->error(nullptr, 0, nullptr, str, make_args()); } fmt::println(str); }; static_assert(true, "")
#define ICHOR_EMERGENCY_LOG2(logger, str, ...) { if(logger != nullptr) { logger->error(nullptr, 0, nullptr, str, make_args(__VA_ARGS__)); } fmt::println(str, __VA_ARGS__); }; static_assert(true, "")
#define ICHOR_EMERGENCY_NO_LOGGER_LOG1(str) { fmt::println(str); }; static_assert(true, "")
#define ICHOR_EMERGENCY_NO_LOGGER_LOG2(str, ...) { fmt::println(str, __VA_ARGS__); }; static_assert(true, "")
#endif

#define ICHOR_OUT_OF_MEM_LOG(bufContainingLog) { std::fwrite(bufContainingLog, 1, sizeof(bufContainingLog) - 1, stdout); std::fflush(nullptr); }
}
