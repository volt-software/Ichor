#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <ichor/services/logging/SpdlogSharedFrameworkLogger.h>

namespace Ichor {
    std::atomic<bool> _setting_logger{false};
    std::atomic<bool> _logger_set{false};

    void _setup_spdlog() {
        bool already_set = _setting_logger.exchange(true);
        if(!already_set) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

            auto time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
            auto file_sink = make_shared<spdlog::sinks::basic_file_sink_mt>(
                    fmt::format("logs/framework-log-{}.txt", time_since_epoch.count()), true);

            auto logger = make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list{console_sink, file_sink});

            logger->set_level(spdlog::level::trace);
            spdlog::flush_on(spdlog::level::err);

            spdlog::set_default_logger(logger);
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
            spdlog::set_pattern("[%C-%m-%d %H:%M:%S.%e] [%s:%#] [%L] %v");
#else
            spdlog::set_pattern("[%C-%m-%d %H:%M:%S.%e] [%L] %v");
#endif
            _logger_set.store(true, std::memory_order_release);
        }
    }
}

Ichor::v1::SpdlogSharedFrameworkLogger::SpdlogSharedFrameworkLogger(Properties props) : AdvancedService(std::move(props)) {
    _setup_spdlog();

    while(!_logger_set.load(std::memory_order_acquire)) {
        // spinlock
    }

    auto logLevelProp = getProperties().find("LogLevel");
    if(logLevelProp != end(getProperties())) {
        setLogLevel(Ichor::v1::any_cast<LogLevel>(logLevelProp->second));
    }

    SPDLOG_TRACE("SpdlogSharedFrameworkLogger constructor");
}

void Ichor::v1::SpdlogSharedFrameworkLogger::trace(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                          std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_TRACE);

    spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::trace, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogSharedFrameworkLogger::debug(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                          std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_DEBUG);

    spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::debug, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogSharedFrameworkLogger::info(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                          std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_INFO);

    spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::info, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogSharedFrameworkLogger::warn(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                          std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_WARN);

    spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::warn, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogSharedFrameworkLogger::error(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                          std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_ERROR);

    spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::err, fmt::vformat(format_str, args));
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::v1::SpdlogSharedFrameworkLogger::start() {
    SPDLOG_TRACE("SpdlogSharedFrameworkLogger started");
    co_return {};
}

Ichor::Task<void> Ichor::v1::SpdlogSharedFrameworkLogger::stop() {
    SPDLOG_TRACE("SpdlogSharedFrameworkLogger stopped");
    co_return;
}

void Ichor::v1::SpdlogSharedFrameworkLogger::setLogLevel(LogLevel level) noexcept {
    _level = level;
}

Ichor::LogLevel Ichor::v1::SpdlogSharedFrameworkLogger::getLogLevel() const noexcept {
    return _level;
}

Ichor::ServiceIdType Ichor::v1::SpdlogSharedFrameworkLogger::getFrameworkServiceId() const noexcept {
    return getServiceId();
}
