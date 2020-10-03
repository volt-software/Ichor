#ifdef USE_SPDLOG

#include <ichor/optional_bundles/logging_bundle/SpdlogSharedService.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

bool Ichor::SpdlogSharedService::start() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    auto time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    auto file_sink = make_shared<spdlog::sinks::basic_file_sink_mt>(fmt::format("logs/log-{}.txt", time_since_epoch.count()), true);

    _sinks = std::vector<std::shared_ptr<spdlog::sinks::sink>>{console_sink, file_sink};
    return true;
}

bool Ichor::SpdlogSharedService::stop() {
    _sinks = {};
    return true;
}

std::vector<std::shared_ptr<spdlog::sinks::sink>>& Ichor::SpdlogSharedService::getSinks() {
    return _sinks;
}

#endif