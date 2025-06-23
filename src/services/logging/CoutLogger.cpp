#include <ichor/services/logging/CoutLogger.h>
#include <ichor/stl/StringUtils.h>
#include <fmt/format.h>
#include <iostream>
#define FMT_INLINE_BUFFER_SIZE 1024

Ichor::CoutLogger::CoutLogger(Properties props) : AdvancedService(std::move(props)) {
    auto logLevelProp = getProperties().find("LogLevel");
    if(logLevelProp != end(getProperties())) {
        setLogLevel(Ichor::any_cast<LogLevel>(logLevelProp->second));
    }
}

void Ichor::CoutLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                         std::string_view format_str, fmt::format_args args) {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_TRACE);

    fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
    if(filename_in != nullptr) {
        const char *base = Ichor::basename(filename_in);
        fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
    }
#endif
    fmt::vformat_to(std::back_inserter(buf), format_str, args);
    std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
}

void Ichor::CoutLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                         std::string_view format_str, fmt::format_args args) {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_DEBUG);

    fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
    if(filename_in != nullptr) {
        const char *base = Ichor::basename(filename_in);
        fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
    }
#endif
    fmt::vformat_to(std::back_inserter(buf), format_str, args);
    std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
}

void Ichor::CoutLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                        std::string_view format_str, fmt::format_args args) {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_INFO);

    fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
    if(filename_in != nullptr) {
        const char *base = Ichor::basename(filename_in);
        fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
    }
#endif
    fmt::vformat_to(std::back_inserter(buf), format_str, args);
    std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
}

void Ichor::CoutLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                        std::string_view format_str, fmt::format_args args) {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_WARN);

    fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
    if(filename_in != nullptr) {
        const char *base = Ichor::basename(filename_in);
        fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
    }
#endif
    fmt::vformat_to(std::back_inserter(buf), format_str, args);
    std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
}

void Ichor::CoutLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                         std::string_view format_str, fmt::format_args args) {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_ERROR);

    fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
    if(filename_in != nullptr) {
        const char *base = Ichor::basename(filename_in);
        fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
    }
#endif
    fmt::vformat_to(std::back_inserter(buf), format_str, args);
    std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
}

void Ichor::CoutLogger::setLogLevel(Ichor::LogLevel level) noexcept {
    _level = level;
}

Ichor::LogLevel Ichor::CoutLogger::getLogLevel() const noexcept {
    return _level;
}
