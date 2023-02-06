#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <iostream>
#define FMT_INLINE_BUFFER_SIZE 1024

Ichor::CoutFrameworkLogger::CoutFrameworkLogger() : IFrameworkLogger(), AdvancedService(), _level(LogLevel::LOG_WARN) {
    std::cout << "CoutFrameworkLogger constructor\n";

    auto logLevelProp = getProperties().find("LogLevel");
    if(logLevelProp != end(getProperties())) {
        setLogLevel(Ichor::any_cast<LogLevel>(logLevelProp->second));
    }
}

void Ichor::CoutFrameworkLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_TRACE) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
        fmt::vformat_to(std::back_inserter(buf), format_str, args);
        std::cout.write(buf.data(), static_cast<int64_t>(buf.size())) << "\n";
    }
}

void Ichor::CoutFrameworkLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_DEBUG) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
        fmt::vformat_to(std::back_inserter(buf), format_str, args);
        std::cout.write(buf.data(), static_cast<int64_t>(buf.size())) << "\n";
    }
}

void Ichor::CoutFrameworkLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_INFO) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
        fmt::vformat_to(std::back_inserter(buf), format_str, args);
        std::cout.write(buf.data(), static_cast<int64_t>(buf.size())) << "\n";
    }
}

void Ichor::CoutFrameworkLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_WARN) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
        fmt::vformat_to(std::back_inserter(buf), format_str, args);
        std::cout.write(buf.data(), static_cast<int64_t>(buf.size())) << "\n";
    }
}

void Ichor::CoutFrameworkLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_ERROR) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
        fmt::vformat_to(std::back_inserter(buf), format_str, args);
        std::cout.write(buf.data(), static_cast<int64_t>(buf.size())) << "\n";
    }
}

Ichor::AsyncGenerator<tl::expected<void, Ichor::StartError>> Ichor::CoutFrameworkLogger::start() {
    std::cout << "CoutFrameworkLogger started\n";
    co_return {};
}

Ichor::AsyncGenerator<void> Ichor::CoutFrameworkLogger::stop() {
    std::cout << "CoutFrameworkLogger stopped\n";
    co_return;
}

void Ichor::CoutFrameworkLogger::setLogLevel(Ichor::LogLevel level) {
    _level = level;
}

Ichor::LogLevel Ichor::CoutFrameworkLogger::getLogLevel() const {
    return _level;
}
