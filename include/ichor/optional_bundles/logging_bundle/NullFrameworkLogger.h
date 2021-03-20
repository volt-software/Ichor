#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/Service.h>

namespace Ichor {

    class NullFrameworkLogger final : public IFrameworkLogger, public Service<NullFrameworkLogger> {
    public:
        NullFrameworkLogger() = default;
        ~NullFrameworkLogger() final = default;

        void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}

        void setLogLevel(LogLevel level) final {}
        [[nodiscard]] LogLevel getLogLevel() const final { return LogLevel::ERROR; }
    };
}