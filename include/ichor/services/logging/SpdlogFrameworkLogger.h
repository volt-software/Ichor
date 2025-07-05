#pragma once

#ifdef ICHOR_USE_SPDLOG

#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/dependency_management/AdvancedService.h>

namespace Ichor::v1 {

    class SpdlogFrameworkLogger final : public IFrameworkLogger, public AdvancedService<SpdlogFrameworkLogger> {
    public:
        SpdlogFrameworkLogger(Properties props);
        ~SpdlogFrameworkLogger() final = default;

        void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;

        void setLogLevel(LogLevel level)  noexcept final;
        [[nodiscard]] LogLevel getLogLevel() const noexcept final;
        [[nodiscard]] ServiceIdType getFrameworkServiceId() const noexcept final;
    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        friend DependencyRegister;

        LogLevel _level;
    };
}

#endif //ICHOR_USE_SPDLOG
