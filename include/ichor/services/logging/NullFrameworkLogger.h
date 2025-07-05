#pragma once

#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/dependency_management/AdvancedService.h>

namespace Ichor::v1 {

    class NullFrameworkLogger final : public IFrameworkLogger, public AdvancedService<NullFrameworkLogger> {
    public:
        NullFrameworkLogger() = default;
        ~NullFrameworkLogger() final = default;

        void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}
        void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final {}

        void setLogLevel(LogLevel level) noexcept final {}
        [[nodiscard]] LogLevel getLogLevel() const noexcept final { return LogLevel::LOG_ERROR; }
        ServiceIdType getFrameworkServiceId() const noexcept final { return getServiceId(); };
    };
}
