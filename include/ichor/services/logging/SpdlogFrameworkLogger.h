#pragma once

#ifdef ICHOR_USE_SPDLOG

#include <unordered_map>
#include <vector>
#include <memory>
#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/Service.h>

namespace Ichor {

    class SpdlogFrameworkLogger final : public IFrameworkLogger, public Service<SpdlogFrameworkLogger> {
    public:
        SpdlogFrameworkLogger(Properties props, DependencyManager *mng);
        ~SpdlogFrameworkLogger() final = default;

        void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;

        void setLogLevel(LogLevel level) final;
        [[nodiscard]] LogLevel getLogLevel() const final;

        StartBehaviour start() final;
        StartBehaviour stop() final;
    private:
        LogLevel _level;
        static std::atomic<bool> _setting_logger;
        static std::atomic<bool> _logger_set;
    };
}

#endif //ICHOR_USE_SPDLOG