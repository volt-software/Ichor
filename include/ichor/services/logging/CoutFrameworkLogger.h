#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/Service.h>

namespace Ichor {

    class CoutFrameworkLogger final : public IFrameworkLogger, public Service<CoutFrameworkLogger> {
    public:
        CoutFrameworkLogger();
        ~CoutFrameworkLogger() final = default;

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