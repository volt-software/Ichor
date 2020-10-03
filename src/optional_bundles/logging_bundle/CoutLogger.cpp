#include <ichor/optional_bundles/logging_bundle/CoutLogger.h>
#include <iostream>

Ichor::CoutLogger::CoutLogger() : ILogger(), Service(), _level(LogLevel::TRACE) {
}

void Ichor::CoutLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                         std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::TRACE) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

void Ichor::CoutLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                         std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::DEBUG) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

void Ichor::CoutLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                        std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::INFO) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

void Ichor::CoutLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                        std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::WARN) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

void Ichor::CoutLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                         std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::ERROR) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

bool Ichor::CoutLogger::start() {
    return true;
}

bool Ichor::CoutLogger::stop() {
    return true;
}

void Ichor::CoutLogger::setLogLevel(Ichor::LogLevel level) {
    _level = level;
}

Ichor::LogLevel Ichor::CoutLogger::getLogLevel() const {
    return _level;
}