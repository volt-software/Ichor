#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <ichor/services/logging/SpdlogSeparateFrameworkLogger.h>
#include <ichor/DependencyManager.h>


Ichor::v1::SpdlogSeparateFrameworkLogger::SpdlogSeparateFrameworkLogger(Properties props) : AdvancedService(std::move(props)), _level(LogLevel::LOG_WARN) {
    if(auto propIt = getProperties().find("SpdlogLog"); propIt == getProperties().end()) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        auto time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
        auto file_sink = make_shared<spdlog::sinks::basic_file_sink_mt>(
                fmt::format("logs/framework-log-{}-{}.txt", GetThreadLocalManager().getId(), time_since_epoch.count()), true);

        _logger = make_reference_counted<spdlog::logger>("multi_sink", spdlog::sinks_init_list{console_sink, file_sink});

        _logger->set_level(spdlog::level::trace);
        _logger->flush_on(spdlog::level::err);;
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
        _logger->set_pattern("[%C-%m-%d %H:%M:%S.%e] [%s:%#] [%L] %v");
#else
        _logger->set_pattern("[%C-%m-%d %H:%M:%S.%e] [%L] %v");
#endif
    } else {
        _logger = any_cast<ReferenceCountedPointer<spdlog::logger>>(propIt->second);
    }

    if(auto logLevelProp = getProperties().find("LogLevel"); logLevelProp == getProperties().end()) {
        setLogLevel(Ichor::v1::any_cast<LogLevel>(logLevelProp->second));
    }

    SPDLOG_TRACE("SpdlogSeparateFrameworkLogger constructor");
}

void Ichor::v1::SpdlogSeparateFrameworkLogger::trace(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                          std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_TRACE);

    _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::trace, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogSeparateFrameworkLogger::debug(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                          std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_DEBUG);

    _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::debug, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogSeparateFrameworkLogger::info(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                          std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_INFO);

    _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::info, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogSeparateFrameworkLogger::warn(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                          std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_WARN);

    _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::warn, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogSeparateFrameworkLogger::error(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                          std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_ERROR);

    _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::err, fmt::vformat(format_str, args));
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::v1::SpdlogSeparateFrameworkLogger::start() {
    SPDLOG_TRACE("SpdlogSeparateFrameworkLogger for dm {} started", GetThreadLocalManager().getId());
    co_return {};
}

Ichor::Task<void> Ichor::v1::SpdlogSeparateFrameworkLogger::stop() {
    SPDLOG_TRACE("SpdlogSeparateFrameworkLogger for dm {} stopped", GetThreadLocalManager().getId());
    co_return;
}

void Ichor::v1::SpdlogSeparateFrameworkLogger::setLogLevel(LogLevel level) noexcept {
    _level = level;
}

Ichor::LogLevel Ichor::v1::SpdlogSeparateFrameworkLogger::getLogLevel() const noexcept {
    return _level;
}

Ichor::ServiceIdType Ichor::v1::SpdlogSeparateFrameworkLogger::getFrameworkServiceId() const noexcept {
    return getServiceId();
}
