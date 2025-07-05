#ifdef ICHOR_USE_SPDLOG

#include <spdlog/spdlog.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/services/logging/SpdlogLogger.h>

Ichor::v1::SpdlogLogger::SpdlogLogger(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<ISpdlogSharedService>(this, DependencyFlags::REQUIRED);

    auto logLevelProp = getProperties().find("LogLevel");
    if(logLevelProp != end(getProperties())) {
        setLogLevel(Ichor::v1::any_cast<LogLevel>(logLevelProp->second));
    }
}

Ichor::v1::SpdlogLogger::~SpdlogLogger() {
    delete static_cast<spdlog::logger*>(_logger);
    _logger = nullptr;
}

void Ichor::v1::SpdlogLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                std::string_view format_str, fmt::format_args args) {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_TRACE);

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_logger == nullptr) {
        std::terminate();
    }
#endif

    static_cast<spdlog::logger*>(_logger)->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::trace, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_DEBUG);

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_logger == nullptr) {
        std::terminate();
    }
#endif

    static_cast<spdlog::logger*>(_logger)->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::debug, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_INFO);

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_logger == nullptr) {
        std::terminate();
    }
#endif

    static_cast<spdlog::logger*>(_logger)->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::info, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_WARN);

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_logger == nullptr) {
        std::terminate();
    }
#endif

    static_cast<spdlog::logger*>(_logger)->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::warn, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    ICHOR_CONTRACT_ASSERT(_level <= LogLevel::LOG_ERROR);

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_logger == nullptr) {
        std::terminate();
    }
#endif

    static_cast<spdlog::logger*>(_logger)->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::err, fmt::vformat(format_str, args));
}

void Ichor::v1::SpdlogLogger::setLogLevel(LogLevel level) noexcept {
    _level = level;
}

Ichor::LogLevel Ichor::v1::SpdlogLogger::getLogLevel() const noexcept {
    return _level;
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::v1::SpdlogLogger::start() {
    auto const &sinks = _sharedService->getSinks();
    _logger = new spdlog::logger("multi_sink", sinks.begin(), sinks.end());

#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
    static_cast<spdlog::logger*>(_logger)->set_pattern("[%C-%m-%d %H:%M:%S.%e] [%s:%#] [%L] %v");
#else
    static_cast<spdlog::logger*>(_logger)->set_pattern("[%C-%m-%d %H:%M:%S.%e] [%L] %v");
#endif

    auto requestedLevelIt = _properties.find("LogLevel");
    if(requestedLevelIt != end(_properties)) {
        _level = Ichor::v1::any_cast<LogLevel>(requestedLevelIt->second);
    } else {
        _level = LogLevel::LOG_INFO;
    }
    static_cast<spdlog::logger*>(_logger)->set_level(spdlog::level::trace);
    co_return {};
}

Ichor::Task<void> Ichor::v1::SpdlogLogger::stop() {
    delete static_cast<spdlog::logger*>(_logger);
    _logger = nullptr;
    co_return;
}

void Ichor::v1::SpdlogLogger::addDependencyInstance(ISpdlogSharedService &shared, IService&) noexcept {
    _sharedService = &shared;
}

void Ichor::v1::SpdlogLogger::removeDependencyInstance(ISpdlogSharedService&, IService&) noexcept {
    _sharedService = nullptr;
}


#endif //ICHOR_USE_SPDLOG
