#ifdef USE_SPDLOG

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <ichor/optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <ichor/DependencyManager.h>

std::atomic<bool> Ichor::SpdlogFrameworkLogger::_setting_logger{false};
std::atomic<bool> Ichor::SpdlogFrameworkLogger::_logger_set{false};

Ichor::SpdlogFrameworkLogger::SpdlogFrameworkLogger(Properties props, DependencyManager *mng) : Service(std::move(props), mng), _level(LogLevel::TRACE) {
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
#ifndef REMOVE_SOURCE_NAMES_FROM_LOGGING
        spdlog::set_pattern("[%C-%m-%d %H:%M:%S.%e] [%s:%#] [%L] %v");
#else
        spdlog::set_pattern("[%C-%m-%d %H:%M:%S.%e] [%L] %v");
#endif
        _logger_set.store(true, std::memory_order_release);
    }

    while(!_logger_set.load(std::memory_order_acquire)) {
        // spinlock
    }

    SPDLOG_TRACE("SpdlogFrameworkLogger constructor");
}

void Ichor::SpdlogFrameworkLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::TRACE) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::trace, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogFrameworkLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::DEBUG) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::debug, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogFrameworkLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::INFO) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::info, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogFrameworkLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::WARN) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::warn, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogFrameworkLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::ERROR) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::err, fmt::vformat(format_str, args));
    }
}

bool Ichor::SpdlogFrameworkLogger::start() {
    SPDLOG_TRACE("SpdlogFrameworkLogger started");
    return true;
}

bool Ichor::SpdlogFrameworkLogger::stop() {
    SPDLOG_TRACE("SpdlogFrameworkLogger stopped");
    return true;
}

void Ichor::SpdlogFrameworkLogger::setLogLevel(Ichor::LogLevel level) {
    _level = level;
}

Ichor::LogLevel Ichor::SpdlogFrameworkLogger::getLogLevel() const {
    return _level;
}


#endif //USE_SPDLOG