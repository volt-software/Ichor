#pragma once

#ifdef ICHOR_USE_SPDLOG

#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/logging/SpdlogSharedService.h>
#include <ichor/ScopedServiceProxy.h>

namespace Ichor::v1 {
    class SpdlogLogger final : public ILogger, public AdvancedService<SpdlogLogger> {
    public:
        SpdlogLogger(DependencyRegister &reg, Properties props);
        ~SpdlogLogger() final;

        void trace(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;
        void debug(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;
        void info(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;
        void warn(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;
        void error(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in, std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS final;

        void addDependencyInstance(Ichor::ScopedServiceProxy<ISpdlogSharedService*> shared, IService &isvc) noexcept;
        void removeDependencyInstance(Ichor::ScopedServiceProxy<ISpdlogSharedService*> shared, IService &isvc) noexcept;

        void setLogLevel(LogLevel level) noexcept final;
        [[nodiscard]] LogLevel getLogLevel() const noexcept final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        friend DependencyRegister;

        void* _logger{};
        LogLevel _level{LogLevel::LOG_TRACE};
        Ichor::ScopedServiceProxy<ISpdlogSharedService*> _sharedService {};
    };
}

#endif
