#pragma once

#include <ichor/services/logging/Logger.h>

namespace Ichor::v1 {

    class NullLogger final : public ILogger {
    public:
        void trace(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final {}
        void debug(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final {}
        void info(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final {}
        void warn(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final {}
        void error(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final {}

        void setLogLevel(LogLevel level) noexcept final {}
        [[nodiscard]] LogLevel getLogLevel() const noexcept final { return LogLevel::LOG_ERROR; }
    };
}
