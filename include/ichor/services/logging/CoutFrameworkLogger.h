#pragma once

#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/dependency_management/AdvancedService.h>

namespace Ichor::v1 {

    class CoutFrameworkLogger final : public IFrameworkLogger, public AdvancedService<CoutFrameworkLogger> {
    public:
        CoutFrameworkLogger();
        ~CoutFrameworkLogger() final = default;

        void trace(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;
        void debug(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;
        void info(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;
        void warn(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;
        void error(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;

        void setLogLevel(LogLevel level) noexcept final;
        [[nodiscard]] LogLevel getLogLevel() const noexcept final;
        [[nodiscard]] ServiceIdType getFrameworkServiceId() const noexcept final;
    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        friend DependencyRegister;

        LogLevel _level;
    };
}
