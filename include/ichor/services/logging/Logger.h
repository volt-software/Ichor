#pragma once

#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/Enums.h>

namespace Ichor::v1 {
    class ILogger {
    public:
        virtual void trace(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS = 0;
        virtual void debug(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS = 0;
        virtual void info(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS = 0;
        virtual void warn(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS = 0;
        virtual void error(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS = 0;

        virtual void setLogLevel(LogLevel level) noexcept = 0;
        [[nodiscard]] virtual LogLevel getLogLevel() const noexcept = 0;

    protected:
        ~ILogger() = default;
    };
}
