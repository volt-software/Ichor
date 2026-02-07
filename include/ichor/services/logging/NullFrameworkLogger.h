#pragma once

#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/dependency_management/AdvancedService.h>

namespace Ichor::v1 {

    class NullFrameworkLogger final : public IFrameworkLogger, public AdvancedService<NullFrameworkLogger> {
    public:
        NullFrameworkLogger() = default;
        ~NullFrameworkLogger() final = default;

        void trace(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final {}
        void debug(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final {}
        void info(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final {}
        void warn(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final {}
        void error(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final {}

        void setLogLevel(LogLevel level) noexcept final {}
        [[nodiscard]] LogLevel getLogLevel() const noexcept final { return LogLevel::LOG_ERROR; }
        ServiceIdType getFrameworkServiceId() const noexcept final { return getServiceId(); };
    };
}
