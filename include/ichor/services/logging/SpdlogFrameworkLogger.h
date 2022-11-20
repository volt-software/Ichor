#pragma once

#ifdef ICHOR_USE_SPDLOG

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
    private:
        AsyncGenerator<void> start() final;
        AsyncGenerator<void> stop() final;

        friend DependencyRegister;

        LogLevel _level;
    };
}

#endif //ICHOR_USE_SPDLOG