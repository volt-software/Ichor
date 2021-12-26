#ifdef USE_SPDLOG

#include <ichor/optional_bundles/logging_bundle/SpdlogSharedService.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <ichor/DependencyManager.h>

Ichor::StartBehaviour Ichor::SpdlogSharedService::start() {
    auto console_sink = std::allocate_shared<spdlog::sinks::stdout_color_sink_st, std::pmr::polymorphic_allocator<>>(getMemoryResource());

    auto time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    auto file_sink = make_shared<spdlog::sinks::basic_file_sink_st>(fmt::format("logs/log-{}-{}.txt", getManager()->getId(), time_since_epoch.count()), true);

    _sinks = std::pmr::vector<std::shared_ptr<spdlog::sinks::sink>>{getMemoryResource()};
    _sinks.emplace_back(std::move(console_sink));
    _sinks.emplace_back(std::move(file_sink));
    return Ichor::StartBehaviour::SUCCEEDED;
}

Ichor::StartBehaviour Ichor::SpdlogSharedService::stop() {
    _sinks.clear();
    return Ichor::StartBehaviour::SUCCEEDED;
}

std::pmr::vector<std::shared_ptr<spdlog::sinks::sink>> const& Ichor::SpdlogSharedService::getSinks() noexcept {
    return _sinks;
}

#endif