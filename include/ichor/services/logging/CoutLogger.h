#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/logging/Logger.h>

namespace Ichor::v1 {
    class CoutLogger final : public ILogger, public AdvancedService<CoutLogger> {
    public:
        CoutLogger(Properties props);

        void trace(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;
        void debug(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;
        void info(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;
        void warn(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;
        void error(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;

        void setLogLevel(LogLevel level) noexcept final;
        [[nodiscard]] LogLevel getLogLevel() const noexcept final;

    private:
        LogLevel _level{LogLevel::LOG_WARN};
    };
}
