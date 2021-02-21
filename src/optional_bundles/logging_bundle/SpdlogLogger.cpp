#ifdef USE_SPDLOG

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <ichor/optional_bundles/logging_bundle/SpdlogLogger.h>
#include <ichor/DependencyManager.h>

Ichor::SpdlogLogger::SpdlogLogger(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ISpdlogSharedService>(this, true);
}

void Ichor::SpdlogLogger::trace(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::TRACE) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::trace, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogLogger::debug(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::DEBUG) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::debug, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogLogger::info(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::INFO) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::info, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogLogger::warn(const char *filename_in, int line_in, const char *funcname_in,
                                          std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::WARN) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::warn, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogLogger::error(const char *filename_in, int line_in, const char *funcname_in,
                                           std::string_view format_str, fmt::format_args args) {
    if(_level <= LogLevel::ERROR) {
        spdlog::log(spdlog::source_loc{filename_in, line_in, funcname_in}, spdlog::level::err, fmt::vformat(format_str, args));
    }
}

void Ichor::SpdlogLogger::setLogLevel(LogLevel level) {
    _level = level;
}

Ichor::LogLevel Ichor::SpdlogLogger::getLogLevel() const {
    return _level;
}

bool Ichor::SpdlogLogger::start() {
    auto const &sinks = _sharedService->getSinks();
    _logger = std::allocate_shared<spdlog::logger, std::pmr::polymorphic_allocator<>>(getManager()->getMemoryResource(), "multi_sink", sinks.begin(), sinks.end());

#ifndef REMOVE_SOURCE_NAMES_FROM_LOGGING
    _logger->set_pattern("[%C-%m-%d %H:%M:%S.%e] [%s:%#] [%L] %v");
#else
    _logger->set_pattern("[%C-%m-%d %H:%M:%S.%e] [%L] %v");
#endif
    _logger->set_level(spdlog::level::trace);


    auto requestedLevelIt = _properties.find("LogLevel");
    if(requestedLevelIt != end(_properties)) {
        auto requestedLevel = Ichor::any_cast<LogLevel>(requestedLevelIt->second);
        if (requestedLevel == LogLevel::TRACE) {
            _logger->set_level(spdlog::level::trace);
        } else if (requestedLevel == LogLevel::DEBUG) {
            _logger->set_level(spdlog::level::debug);
        } else if (requestedLevel == LogLevel::INFO) {
            _logger->set_level(spdlog::level::info);
        } else if (requestedLevel == LogLevel::WARN) {
            _logger->set_level(spdlog::level::warn);
        } else if (requestedLevel == LogLevel::ERROR) {
            _logger->set_level(spdlog::level::err);
        } else {
            _logger->set_level(spdlog::level::info);
        }
    } else {
        _logger->set_level(spdlog::level::info);
    }

    auto targetServiceId = Ichor::any_cast<uint64_t>(_properties["TargetServiceId"]);
    _logger->trace("SpdlogLogger {} started for component {}", getServiceId(), targetServiceId);
    return true;
}

bool Ichor::SpdlogLogger::stop() {
    auto targetServiceId = Ichor::any_cast<uint64_t>(_properties["TargetServiceId"]);
    _logger->trace("SpdlogLogger {} stopped for component {}", getServiceId(), targetServiceId);
    return true;
}

void Ichor::SpdlogLogger::addDependencyInstance(ISpdlogSharedService *shared) noexcept {
    _sharedService = shared;
}

void Ichor::SpdlogLogger::removeDependencyInstance(ISpdlogSharedService *) noexcept {
    _sharedService = nullptr;
}


#endif //USE_SPDLOG