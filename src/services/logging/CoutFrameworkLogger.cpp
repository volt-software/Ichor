#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <ichor/stl/StringUtils.h>
#include <iostream>
#define FMT_INLINE_BUFFER_SIZE 1024

Ichor::CoutFrameworkLogger::CoutFrameworkLogger() : IFrameworkLogger(), AdvancedService(), _level(LogLevel::LOG_WARN) {
    auto logLevelProp = getProperties().find("LogLevel");
    if(logLevelProp != end(getProperties())) {
        setLogLevel(Ichor::any_cast<LogLevel>(logLevelProp->second));
    }

    ICHOR_LOG_TRACE(this, "CoutFrameworkLogger constructor");
}

void Ichor::CoutFrameworkLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
//    std::cout << "trace \"" << filename_in << "\" \"" << format_str << "\" " << (int)_level << "\n";
    if(_level <= LogLevel::LOG_TRACE) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
        if(filename_in != nullptr) {
            const char *base = Ichor::basename(filename_in);
            fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
        }
#endif
        fmt::vformat_to(std::back_inserter(buf), format_str, args);
        std::cout.write(buf.data(), static_cast<int64_t>(buf.size())) << "\n";
    }
}

void Ichor::CoutFrameworkLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_DEBUG) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
        if(filename_in != nullptr) {
            const char *base = Ichor::basename(filename_in);
            fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
        }
#endif
        fmt::vformat_to(std::back_inserter(buf), format_str, args);
        std::cout.write(buf.data(), static_cast<int64_t>(buf.size())) << "\n";
    }
}

void Ichor::CoutFrameworkLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_INFO) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
        if(filename_in != nullptr) {
            const char *base = Ichor::basename(filename_in);
            fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
        }
#endif
        fmt::vformat_to(std::back_inserter(buf), format_str, args);
        std::cout.write(buf.data(), static_cast<int64_t>(buf.size())) << "\n";
    }
}

void Ichor::CoutFrameworkLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_WARN) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
        if(filename_in != nullptr) {
            const char *base = Ichor::basename(filename_in);
            fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
        }
#endif
        fmt::vformat_to(std::back_inserter(buf), format_str, args);
        std::cout.write(buf.data(), static_cast<int64_t>(buf.size())) << "\n";
    }
}

void Ichor::CoutFrameworkLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_ERROR) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
        if(filename_in != nullptr) {
            const char *base = Ichor::basename(filename_in);
            fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
        }
#endif
        fmt::vformat_to(std::back_inserter(buf), format_str, args);
        std::cout.write(buf.data(), static_cast<int64_t>(buf.size())) << "\n";
    }
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::CoutFrameworkLogger::start() {

    ICHOR_LOG_TRACE(this, "CoutFrameworkLogger started");
    co_return {};
}

Ichor::Task<void> Ichor::CoutFrameworkLogger::stop() {

    ICHOR_LOG_TRACE(this, "CoutFrameworkLogger stopped");
    co_return;
}

void Ichor::CoutFrameworkLogger::setLogLevel(Ichor::LogLevel level) {
    _level = level;
}

Ichor::LogLevel Ichor::CoutFrameworkLogger::getLogLevel() const {
    return _level;
}
