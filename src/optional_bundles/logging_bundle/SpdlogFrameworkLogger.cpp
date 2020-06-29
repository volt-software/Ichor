#ifdef USE_SPDLOG

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "optional_bundles/logging_bundle/SpdlogFrameworkLogger.h"

Cppelix::SpdlogFrameworkLogger::SpdlogFrameworkLogger() : IFrameworkLogger(), Service() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    auto time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    auto file_sink = make_shared<spdlog::sinks::basic_file_sink_mt>(fmt::format("logs/log-{}.txt", time_since_epoch.count()), true);

    auto logger = make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list{console_sink, file_sink});

    logger->set_level(spdlog::level::trace);

    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%C-%m-%d %H:%M:%S.%e] [%s:%#] [%L] %v");

    SPDLOG_TRACE("Starting SpdlogFrameworkLogger");
}

void Cppelix::SpdlogFrameworkLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(spdlog::default_logger()->should_log(spdlog::level::trace)) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::trace, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogFrameworkLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(spdlog::default_logger()->should_log(spdlog::level::debug)) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::debug, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogFrameworkLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(spdlog::default_logger()->should_log(spdlog::level::info)) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::info, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogFrameworkLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(spdlog::default_logger()->should_log(spdlog::level::warn)) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::warn, fmt::vformat(format_str, args));
    }
}

void Cppelix::SpdlogFrameworkLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(spdlog::default_logger()->should_log(spdlog::level::err)) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::err, fmt::vformat(format_str, args));
    }
}

bool Cppelix::SpdlogFrameworkLogger::start() {
    SPDLOG_TRACE("SpdlogFrameworkLogger started");
    return true;
}

bool Cppelix::SpdlogFrameworkLogger::stop() {
    SPDLOG_TRACE("SpdlogFrameworkLogger stopped");
    return true;
}


#endif //USE_SPDLOG