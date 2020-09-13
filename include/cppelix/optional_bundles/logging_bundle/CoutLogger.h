#pragma once

#include <memory>
#include <cppelix/interfaces/IFrameworkLogger.h>
#include <cppelix/Service.h>
#include <cppelix/optional_bundles/logging_bundle/Logger.h>

namespace Cppelix {
    class CoutLogger final : public ILogger, public Service {
    public:
        CoutLogger();
        bool start() final;
        bool stop() final;

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