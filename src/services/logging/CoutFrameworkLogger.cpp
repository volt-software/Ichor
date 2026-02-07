#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <ichor/stl/StringUtils.h>
#include <fmt/format.h>
#include <iostream>
#define FMT_INLINE_BUFFER_SIZE 1024

Ichor::v1::CoutFrameworkLogger::CoutFrameworkLogger() : IFrameworkLogger(), AdvancedService(), _level(LogLevel::LOG_WARN) {
    auto logLevelProp = getProperties().find("LogLevel");
    if(logLevelProp != end(getProperties())) {
        setLogLevel(Ichor::v1::any_cast<LogLevel>(logLevelProp->second));
    }

    // preventing a warnig on -Wnonnull-compare
    CoutFrameworkLogger *logger = this;
    ICHOR_LOG_TRACE(logger, "CoutFrameworkLogger constructor");
}

void Ichor::v1::CoutFrameworkLogger::trace(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                           std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_TRACE);

    fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
    if(filename_in != nullptr) {
        const char *base = Ichor::v1::basename(filename_in);
        fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
    }
#endif
    fmt::vformat_to(std::back_inserter(buf), format_str, args);
    std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
}

void Ichor::v1::CoutFrameworkLogger::debug(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                           std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_DEBUG);

    fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
    if(filename_in != nullptr) {
        const char *base = Ichor::v1::basename(filename_in);
        fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
    }
#endif
    fmt::vformat_to(std::back_inserter(buf), format_str, args);
    std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
}

void Ichor::v1::CoutFrameworkLogger::info(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                          std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_INFO);

    fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
    if(filename_in != nullptr) {
        const char *base = Ichor::v1::basename(filename_in);
        fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
    }
#endif
    fmt::vformat_to(std::back_inserter(buf), format_str, args);
    std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
}

void Ichor::v1::CoutFrameworkLogger::warn(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                          std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_WARN);

    fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
    if(filename_in != nullptr) {
        const char *base = Ichor::v1::basename(filename_in);
        fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
    }
#endif
    fmt::vformat_to(std::back_inserter(buf), format_str, args);
    std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
}

void Ichor::v1::CoutFrameworkLogger::error(const char * ICHOR_RESTRICT filename_in, int line_in, const char * ICHOR_RESTRICT funcname_in,
                                           std::string_view format_str, fmt::format_args args) ICHOR_RESTRICT_THIS {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_ERROR);

    fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
    if(filename_in != nullptr) {
        const char *base = Ichor::v1::basename(filename_in);
        fmt::vformat_to(std::back_inserter(buf), "[{}:{}] ", fmt::make_format_args(base, line_in));
    }
#endif
    fmt::vformat_to(std::back_inserter(buf), format_str, args);
    std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::v1::CoutFrameworkLogger::start() {
    ICHOR_LOG_TRACE(this, "CoutFrameworkLogger started");
    co_return {};
}

Ichor::Task<void> Ichor::v1::CoutFrameworkLogger::stop() {
    ICHOR_LOG_TRACE(this, "CoutFrameworkLogger stopped");
    co_return;
}

void Ichor::v1::CoutFrameworkLogger::setLogLevel(LogLevel level) noexcept {
    _level = level;
}

Ichor::LogLevel Ichor::v1::CoutFrameworkLogger::getLogLevel() const noexcept {
    return _level;
}

Ichor::ServiceIdType Ichor::v1::CoutFrameworkLogger::getFrameworkServiceId() const noexcept {
    return getServiceId();
}
