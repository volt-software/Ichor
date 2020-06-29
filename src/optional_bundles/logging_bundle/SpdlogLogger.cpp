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
    if(_logger->should_log(spdlog::level::trace)) {
        _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::trace, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_logger->should_log(spdlog::level::debug)) {
        _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::debug, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_logger->should_log(spdlog::level::info)) {
        _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::info, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_logger->should_log(spdlog::level::warn)) {
        _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::warn, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_logger->should_log(spdlog::level::err)) {
        _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::err, fmt::vformat(format_str, args));
    }
}

bool Cppelix::SpdlogLogger::start() {
    auto requestedLevel = _properties["LogLevel"]->getAsString();
    if(requestedLevel == "trace") {
        _logger->set_level(spdlog::level::trace);
    } else if(requestedLevel == "debug") {
        _logger->set_level(spdlog::level::debug);
    } else if(requestedLevel == "info") {
        _logger->set_level(spdlog::level::info);
    } else if(requestedLevel == "warn") {
        _logger->set_level(spdlog::level::warn);
    } else if(requestedLevel == "error") {
        _logger->set_level(spdlog::level::err);
    } else {
        _logger->set_level(spdlog::level::info);
    }

    auto nameHash = _properties["ServiceNameHash"]->getAsLong();
    SPDLOG_TRACE("SpdlogLogger started for component {}", nameHash);
    return true;
}

bool Cppelix::SpdlogLogger::stop() {
    auto nameHash = _properties["ServiceNameHash"]->getAsLong();
    SPDLOG_TRACE("SpdlogLogger stopped for component {}", nameHash);
    return true;
}


#endif //USE_SPDLOG