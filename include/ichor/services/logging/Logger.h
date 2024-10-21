#pragma once

#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/Enums.h>

namespace Ichor {
    class ILogger {
    public:
        virtual void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;
        virtual void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) = 0;

        virtual void setLogLevel(LogLevel level) noexcept = 0;
        [[nodiscard]] virtual LogLevel getLogLevel() const noexcept = 0;

    protected:
        ~ILogger() = default;
    };
}
