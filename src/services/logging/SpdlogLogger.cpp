#ifdef ICHOR_USE_SPDLOG

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <ichor/services/logging/SpdlogLogger.h>
#include <ichor/DependencyManager.h>

Ichor::SpdlogLogger::SpdlogLogger(DependencyRegister &reg, Properties props, DependencyManager *mng) : AdvancedService(std::move(props), mng) {
    reg.registerDependency<ISpdlogSharedService>(this, true);

    auto logLevelProp = getProperties().find("LogLevel");
    if(logLevelProp != end(getProperties())) {
        setLogLevel(Ichor::any_cast<LogLevel>(logLevelProp->second));
    }
}

void Ichor::SpdlogLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_TRACE) {
        _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::trace, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_DEBUG) {
        _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::debug, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_INFO) {
        _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::info, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_WARN) {
        _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::warn, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::LOG_ERROR) {
        _logger->log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::err, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogLogger::setLogLevel(LogLevel level) {
    _level = level;
}

Ichor::LogLevel Ichor::SpdlogLogger::getLogLevel() const {
    return _level;
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::SpdlogLogger::start() {
    auto const &sinks = _sharedService->getSinks();
    _logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());

#ifndef ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING
    _logger->set_pattern("[%C-%m-%d %H:%M:%S.%e] [%s:%#] [%L] %v");
#else
    _logger->set_pattern("[%C-%m-%d %H:%M:%S.%e] [%L] %v");
#endif

    auto requestedLevelIt = _properties.find("LogLevel");
    if(requestedLevelIt != end(_properties)) {
        _level = Ichor::any_cast<LogLevel>(requestedLevelIt->second);
    } else {
        _level = Ichor::LogLevel::LOG_INFO;
    }
    _logger->set_level(spdlog::level::trace);
    co_return {};
}

Ichor::Task<void> Ichor::SpdlogLogger::stop() {
    co_return;
}

void Ichor::SpdlogLogger::addDependencyInstance(ISpdlogSharedService *shared, IService *) noexcept {
    _sharedService = shared;
}

void Ichor::SpdlogLogger::removeDependencyInstance(ISpdlogSharedService *, IService *) noexcept {
    _sharedService = nullptr;
}


#endif //ICHOR_USE_SPDLOG
