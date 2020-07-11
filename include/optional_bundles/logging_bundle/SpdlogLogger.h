#pragma once

#ifdef USE_SPDLOG

#include <unordered_map>
#include <vector>
#include <memory>
#include <framework/interfaces/IFrameworkLogger.h>
#include <framework/Service.h>
#include "Logger.h"

namespace Cppelix {
    class SpdlogLogger : public ILogger, public Service {
    public:
        SpdlogLogger();
        bool start() final;
        bool stop() final;

        void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;

        void setLogLevel(LogLevel level) final;
        LogLevel getLogLevel() const final;

    private:
        std::shared_ptr<spdlog::logger> _logger;
        LogLevel _level;
    };
}

#endif