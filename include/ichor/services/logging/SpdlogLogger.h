#pragma once

#ifdef ICHOR_USE_SPDLOG

#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/logging/SpdlogSharedService.h>
#include <ichor/stl/ReferenceCountedPointer.h>
#include <spdlog/spdlog.h>

namespace Ichor {
    class SpdlogLogger final : public ILogger, public AdvancedService<SpdlogLogger> {
    public:
        SpdlogLogger(DependencyRegister &reg, Properties props);

        void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;

        void addDependencyInstance(ISpdlogSharedService &shared, IService &isvc) noexcept;
        void removeDependencyInstance(ISpdlogSharedService &shared, IService &isvc) noexcept;

        void setLogLevel(LogLevel level) noexcept final;
        [[nodiscard]] LogLevel getLogLevel() const noexcept final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        friend DependencyRegister;

        ReferenceCountedPointer<spdlog::logger> _logger{};
        LogLevel _level{LogLevel::LOG_TRACE};
        ISpdlogSharedService* _sharedService{};
    };
}

#endif
