#ifdef ICHOR_USE_SPDLOG

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/SpdlogSharedService.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::SpdlogSharedService::start() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();

    auto time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_st>(fmt::format("logs/log-{}-{}.txt", GetThreadLocalManager().getId(), time_since_epoch.count()), true);

    _sinks = std::vector<std::shared_ptr<spdlog::sinks::sink>>{};
    _sinks.emplace_back(std::move(console_sink));
    _sinks.emplace_back(std::move(file_sink));
    co_return {};
}

Ichor::Task<void> Ichor::SpdlogSharedService::stop() {
    _sinks.clear();
    co_return;
}

std::vector<std::shared_ptr<spdlog::sinks::sink>> const& Ichor::SpdlogSharedService::getSinks() noexcept {
    return _sinks;
}

#endif
