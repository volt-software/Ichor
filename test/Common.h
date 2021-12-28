#pragma once

#include "catch2/catch_test_macros.hpp"
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/NullFrameworkLogger.h>
#if __has_include(<spdlog/spdlog.h>)
#include <ichor/optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include "spdlog/sinks/stdout_color_sinks.h"
#endif

using namespace Ichor;

void waitForRunning(DependencyManager &dm) {
    while(!dm.isRunning()) {
        std::this_thread::sleep_for(1ms);
    }
}

void ensureInternalLoggerExists() {
#if __has_include(<spdlog/spdlog.h>)
    //default logger is disabled in cmake

    if constexpr(!DO_INTERNAL_DEBUG) {
        return;
    }

    if(spdlog::default_logger_raw() == nullptr) {
        auto new_logger = spdlog::stdout_color_st("new_default_logger");
//        new_logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(new_logger);
    }
#endif
}