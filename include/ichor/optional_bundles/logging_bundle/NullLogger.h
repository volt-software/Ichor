#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>

namespace Ichor {

    class NullLogger final : public ILogger, public Service<NullLogger> {
    public:
        NullLogger() = default;
        ~NullLogger() final = default;

        void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}

        void setLogLevel(LogLevel level) final {}
        [[nodiscard]] LogLevel getLogLevel() const final { return LogLevel::ERROR; }
    };
}