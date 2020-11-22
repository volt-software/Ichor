#pragma once

#include <fmt/format.h>
#include <ichor/Common.h>
#include <ichor/Service.h>
#include <ichor/interfaces/LogLevel.h>

namespace Ichor {
    class ILogger : virtual public IService {
    public:
        virtual void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;

        virtual void setLogLevel(LogLevel level) = 0;
        [[nodiscard]] virtual LogLevel getLogLevel() const = 0;

    protected:
        ~ILogger() override = default;
    };

#ifndef LOG_TRACE
#define LOG_TRACE(logger, str, ...) if(logger != nullptr) logger->trace(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#define LOG_DEBUG(logger, str, ...) if(logger != nullptr) logger->debug(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#define LOG_INFO(logger, str, ...) if(logger != nullptr) logger->info(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#define LOG_WARN(logger, str, ...) if(logger != nullptr) logger->warn(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#define LOG_ERROR(logger, str, ...) if(logger != nullptr) logger->error(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#endif
}