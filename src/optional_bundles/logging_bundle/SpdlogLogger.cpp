#ifdef USE_SPDLOG

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "optional_bundles/logging_bundle/SpdlogLogger.h"

Cppelix::SpdlogLogger::SpdlogLogger() : ILogger(), Service() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    auto time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    auto file_sink = make_shared<spdlog::sinks::basic_file_sink_mt>(fmt::format("logs/log-{}.txt", time_since_epoch.count()), true);

    _logger = make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list{console_sink, file_sink});
    _logger->set_pattern("[%C-%m-%d %H:%M:%S.%e] [%s:%#] [%L] %v");
    SPDLOG_TRACE("Constructor SpdlogLogger");
}

void Cppelix::SpdlogLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::TRACE) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::trace, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::DEBUG) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::debug, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::INFO) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::info, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::WARN) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::warn, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::ERROR) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::err, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogLogger::setLogLevel(Cppelix::LogLevel level) {
    _level = level;
}

Cppelix::LogLevel Cppelix::SpdlogLogger::getLogLevel() const {
    return _level;
}

bool Cppelix::SpdlogLogger::start() {
    auto requestedLevelIt = _properties.find("LogLevel");
    if(requestedLevelIt != end(_properties)) {
        auto requestedLevel = std::any_cast<LogLevel>(requestedLevelIt->second);
        if (requestedLevel == LogLevel::TRACE) {
            _logger->set_level(spdlog::level::trace);
        } else if (requestedLevel == LogLevel::DEBUG) {
            _logger->set_level(spdlog::level::debug);
        } else if (requestedLevel == LogLevel::INFO) {
            _logger->set_level(spdlog::level::info);
        } else if (requestedLevel == LogLevel::WARN) {
            _logger->set_level(spdlog::level::warn);
        } else if (requestedLevel == LogLevel::ERROR) {
            _logger->set_level(spdlog::level::err);
        } else {
            _logger->set_level(spdlog::level::info);
        }
    } else {
        _logger->set_level(spdlog::level::info);
    }

    auto targetServiceId = std::any_cast<uint64_t>(_properties["TargetServiceId"]);
    SPDLOG_TRACE("SpdlogLogger {} started for component {}", getServiceId(), targetServiceId);
    return true;
}

bool Cppelix::SpdlogLogger::stop() {
    auto targetServiceId = std::any_cast<uint64_t>(_properties["TargetServiceId"]);
    SPDLOG_TRACE("SpdlogLogger {} stopped for component {}", getServiceId(), targetServiceId);
    return true;
}


#endif //USE_SPDLOG