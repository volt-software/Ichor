#include <cppelix/optional_bundles/logging_bundle/CoutLogger.h>
#include <iostream>

Cppelix::CoutLogger::CoutLogger() : ILogger(), Service(), _level(LogLevel::TRACE) {
}

void Cppelix::CoutLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                         std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::TRACE) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

void Cppelix::CoutLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                         std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::DEBUG) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

void Cppelix::CoutLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                        std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::INFO) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

void Cppelix::CoutLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                        std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::WARN) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

void Cppelix::CoutLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                         std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::ERROR) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

bool Cppelix::CoutLogger::start() {
    return true;
}

bool Cppelix::CoutLogger::stop() {
    return true;
}

void Cppelix::CoutLogger::setLogLevel(Cppelix::LogLevel level) {
    _level = level;
}

Cppelix::LogLevel Cppelix::CoutLogger::getLogLevel() const {
    return _level;
}