#pragma once

#include <memory>
#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/Service.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>

namespace Ichor {
    class CoutLogger final : public ILogger, public Service<CoutLogger> {
    public:
        CoutLogger();

        void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;

        void setLogLevel(LogLevel level) final;
        [[nodiscard]] LogLevel getLogLevel() const final;

    private:
        LogLevel _level;
    };
}