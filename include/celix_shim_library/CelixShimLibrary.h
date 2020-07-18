#pragma once

#define CELIX_DM_DEPENDENCYMANAGER_H

#include <cstdint>
#include <celix_types.h>
#include <celix_errno.h>
#include <framework/DependencyManager.h>
#include <optional_bundles/logging_bundle/Logger.h>
#include "CelixShimService.h"
#include "CelixShimTracker.h"

typedef celix_status_t (*create_function_fp)(bundle_context_t *context, void **userData);
typedef celix_status_t (*start_function_fp)(void *userData, bundle_context_t *context);
typedef celix_status_t (*stop_function_fp)(void *userData, bundle_context_t *context);
typedef celix_status_t (*destroy_function_fp)(void *userData, bundle_context_t *context);

namespace Cppelix {
    struct ICelixBundleContext : virtual public IService {
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
    };

    struct CelixBundleContext final : public ICelixBundleContext, public Service {
        bool start() final {
            LOG_INFO(_logger, "CelixBundleContext started with dependency");
            return true;
        }

        bool stop() final {
            LOG_INFO(_logger, "CelixBundleContext stopped with dependency");
            return true;
        }

        void addDependencyInstance(ILogger *logger) {
            _logger = logger;

            LOG_INFO(logger, "Inserted logger svcid {} for svcid {}", logger->getServiceId(), getServiceId());
        }

        void removeDependencyInstance(ILogger *logger) {
            _logger = nullptr;
        }

        void addDependencyInstance(ICelixShimService *shimService) {

            LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", shimService->getServiceId(), getServiceId());
        }

        void removeDependencyInstance(ICelixShimService *shimService) {
        }

        void setUserData(void *_userData) {
            userData = _userData;
        }

        void* getUserData() {
            return userData;
        }

        static std::atomic<uint64_t> trackerIdCounter;
        static std::unordered_map<uint64_t, Cppelix::IService *> registeredServices;
        static std::unordered_multimap<uint64_t, CelixShimTracker> registeredServiceTrackers;
        static std::mutex m;
        ILogger *_logger;
        void *userData;
    };
}