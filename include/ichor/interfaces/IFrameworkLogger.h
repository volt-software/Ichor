#pragma once

#include <fmt/format.h>
#include <ichor/Common.h>
#include <ichor/Service.h>
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

#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
#define ICHOR_LOG_TRACE(logger, str, ...) if(logger != nullptr) logger->trace(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#define ICHOR_LOG_DEBUG(logger, str, ...) if(logger != nullptr) logger->debug(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#define ICHOR_LOG_INFO(logger, str, ...) if(logger != nullptr) logger->info(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#define ICHOR_LOG_WARN(logger, str, ...) if(logger != nullptr) logger->warn(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#define ICHOR_LOG_ERROR(logger, str, ...) if(logger != nullptr) logger->error(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#else
#define ICHOR_LOG_TRACE(logger, str, ...) if(logger != nullptr) logger->trace(nullptr, 0, nullptr, str, fmt::make_format_args(__VA_ARGS__))
#define ICHOR_LOG_DEBUG(logger, str, ...) if(logger != nullptr) logger->debug(nullptr, 0, nullptr, str, fmt::make_format_args(__VA_ARGS__))
#define ICHOR_LOG_INFO(logger, str, ...) if(logger != nullptr) logger->info(nullptr, 0, nullptr, str, fmt::make_format_args(__VA_ARGS__))
#define ICHOR_LOG_WARN(logger, str, ...) if(logger != nullptr) logger->warn(nullptr, 0, nullptr, str, fmt::make_format_args(__VA_ARGS__))
#define ICHOR_LOG_ERROR(logger, str, ...) if(logger != nullptr) logger->error(nullptr, 0, nullptr, str, fmt::make_format_args(__VA_ARGS__))
#endif
}