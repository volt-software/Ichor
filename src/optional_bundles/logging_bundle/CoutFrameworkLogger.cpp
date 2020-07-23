#include "optional_bundles/logging_bundle/CoutFrameworkLogger.h"
#include <iostream>

Cppelix::CoutFrameworkLogger::CoutFrameworkLogger() : IFrameworkLogger(), Service(), _level(LogLevel::TRACE) {
    std::cout << "CoutFrameworkLogger constructor\n";
}

void Cppelix::CoutFrameworkLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::TRACE) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

void Cppelix::CoutFrameworkLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::DEBUG) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

void Cppelix::CoutFrameworkLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::INFO) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

void Cppelix::CoutFrameworkLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::WARN) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

void Cppelix::CoutFrameworkLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::ERROR) {
        std::cout << fmt::vformat(format_str, args) << "\n";
    }
}

bool Cppelix::CoutFrameworkLogger::start() {
    std::cout << "CoutFrameworkLogger started\n";
    return true;
}

bool Cppelix::CoutFrameworkLogger::stop() {
    std::cout << "CoutFrameworkLogger stopped\n";
    return true;
}

void Cppelix::CoutFrameworkLogger::setLogLevel(Cppelix::LogLevel level) {
    _level = level;
}

Cppelix::LogLevel Cppelix::CoutFrameworkLogger::getLogLevel() const {
    return _level;
}