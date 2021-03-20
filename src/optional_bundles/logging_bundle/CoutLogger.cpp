#include <ichor/optional_bundles/logging_bundle/CoutLogger.h>
#include <iostream>
#define FMT_INLINE_BUFFER_SIZE 1024

Ichor::CoutLogger::CoutLogger() : ILogger(), Service(), _level(LogLevel::TRACE) {
}

void Ichor::CoutLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                         std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::TRACE) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE, std::pmr::polymorphic_allocator<char>> buf{getMemoryResource()};
        fmt::vformat_to(buf, format_str, args);
        std::cout.write(buf.data(), buf.size()) << "\n";
    }
}

void Ichor::CoutLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                         std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::DEBUG) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE, std::pmr::polymorphic_allocator<char>> buf{getMemoryResource()};
        fmt::vformat_to(buf, format_str, args);
        std::cout.write(buf.data(), buf.size()) << "\n";
    }
}

void Ichor::CoutLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                        std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::INFO) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE, std::pmr::polymorphic_allocator<char>> buf{getMemoryResource()};
        fmt::vformat_to(buf, format_str, args);
        std::cout.write(buf.data(), buf.size()) << "\n";
    }
}

void Ichor::CoutLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                        std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::WARN) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE, std::pmr::polymorphic_allocator<char>> buf{getMemoryResource()};
        fmt::vformat_to(buf, format_str, args);
        std::cout.write(buf.data(), buf.size()) << "\n";
    }
}

void Ichor::CoutLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                         std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::ERROR) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE, std::pmr::polymorphic_allocator<char>> buf{getMemoryResource()};
        fmt::vformat_to(buf, format_str, args);
        std::cout.write(buf.data(), buf.size()) << "\n";
    }
}

void Ichor::CoutLogger::setLogLevel(Ichor::LogLevel level) {
    _level = level;
}

Ichor::LogLevel Ichor::CoutLogger::getLogLevel() const {
    return _level;
}