#pragma once

#include <ichor/interfaces/IFrameworkLogger.h>

namespace Ichor {

    class NullFrameworkLogger final : public IFrameworkLogger {
    public:
        void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}

        void setLogLevel(LogLevel level) noexcept final {}
        [[nodiscard]] LogLevel getLogLevel() const noexcept final { return LogLevel::LOG_ERROR; }
    };
}
