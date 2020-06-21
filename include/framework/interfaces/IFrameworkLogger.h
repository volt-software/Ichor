#pragma once

#include <fmt/format.h>
#include "../Common.h"

namespace Cppelix {
    class IFrameworkLogger {
    public:
        virtual void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;

        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
    protected:
        virtual ~IFrameworkLogger() = default;
    };

#define LOG_TRACE(logger, str, ...) logger->trace(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#define LOG_DEBUG(logger, str, ...) logger->debug(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#define LOG_INFO(logger, str, ...) logger->info(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#define LOG_WARN(logger, str, ...) logger->warn(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
#define LOG_ERROR(logger, str, ...) logger->error(__FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), str, fmt::make_format_args(__VA_ARGS__))
}