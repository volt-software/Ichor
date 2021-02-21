#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <iostream>
#define FMT_INLINE_BUFFER_SIZE 1024

Ichor::CoutFrameworkLogger::CoutFrameworkLogger() : IFrameworkLogger(), Service(), _level(LogLevel::TRACE) {
    std::cout << "CoutFrameworkLogger constructor\n";
}

void Ichor::CoutFrameworkLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::TRACE) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE, std::pmr::polymorphic_allocator<char>> buf{getMemoryResource()};
        fmt::vformat_to(buf, format_str, args);
        std::cout.write(buf.data(), buf.size()) << "\n";
    }
}

void Ichor::CoutFrameworkLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::DEBUG) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE, std::pmr::polymorphic_allocator<char>> buf{getMemoryResource()};
        fmt::vformat_to(buf, format_str, args);
        std::cout.write(buf.data(), buf.size()) << "\n";
    }
}

void Ichor::CoutFrameworkLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::INFO) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE, std::pmr::polymorphic_allocator<char>> buf{getMemoryResource()};
        fmt::vformat_to(buf, format_str, args);
        std::cout.write(buf.data(), buf.size()) << "\n";
    }
}

void Ichor::CoutFrameworkLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::WARN) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE, std::pmr::polymorphic_allocator<char>> buf{getMemoryResource()};
        fmt::vformat_to(buf, format_str, args);
        std::cout.write(buf.data(), buf.size()) << "\n";
    }
}

void Ichor::CoutFrameworkLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::ERROR) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE, std::pmr::polymorphic_allocator<char>> buf{getMemoryResource()};
        fmt::vformat_to(buf, format_str, args);
        std::cout.write(buf.data(), buf.size()) << "\n";
    }
}

bool Ichor::CoutFrameworkLogger::start() {
    std::cout << "CoutFrameworkLogger started\n";
    return true;
}

bool Ichor::CoutFrameworkLogger::stop() {
    std::cout << "CoutFrameworkLogger stopped\n";
    return true;
}

void Ichor::CoutFrameworkLogger::setLogLevel(Ichor::LogLevel level) {
    _level = level;
}

Ichor::LogLevel Ichor::CoutFrameworkLogger::getLogLevel() const {
    return _level;
}