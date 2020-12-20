#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>

namespace Ichor {
    struct ILoggerAdmin : virtual public IService {
    };

    template<typename LogT>
    class LoggerAdmin final : public ILoggerAdmin, public Service {
    public:
        LoggerAdmin(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
            reg.registerDependency<IFrameworkLogger>(this, true);
        }
        ~LoggerAdmin() final = default;

        bool start() final {
            _loggerTrackerRegistration = getManager()->template registerDependencyTracker<ILogger>(this);
            return true;
        }

        bool stop() final {
            return true;
        }

        void addDependencyInstance(IFrameworkLogger *logger) noexcept {
            _logger = logger;
            LOG_TRACE(_logger, "Inserted logger");
        }

        void removeDependencyInstance(IFrameworkLogger *logger) noexcept {
            _logger = nullptr;
        }

        void handleDependencyRequest(ILogger *, DependencyRequestEvent const *const evt) {
            auto logger = _loggers.find(evt->originatingService);

//            LOG_ERROR(_logger, "dep req {} dm {}", evt->manager->serviceId(), getManager()->getId());

            auto requestedLevel = LogLevel::INFO;
            if(evt->properties.has_value()) {
                auto requestedLevelIt = evt->properties.value()->find("LogLevel");
                requestedLevel = requestedLevelIt != end(*evt->properties.value()) ? std::any_cast<LogLevel>(requestedLevelIt->second) : LogLevel::INFO;
            }
            if (logger == end(_loggers)) {
//                LOG_ERROR(_logger, "creating logger for svcid {}", evt->originatingService);
                    _loggers.emplace(evt->originatingService, getManager()->template createServiceManager<LogT, ILogger>(
                            IchorProperties{{"LogLevel",        requestedLevel},
                                              {"TargetServiceId", evt->originatingService},
                                              {"Filter",          Filter{ServiceIdFilterEntry{evt->originatingService}}}}));
            } else {
                LOG_TRACE(_logger, "svcid {} already has logger", evt->originatingService);
            }
        }

        void handleDependencyUndoRequest(ILogger *, DependencyUndoRequestEvent const *const evt) {
            _loggers.erase(evt->originatingService);
        }

    private:
        IFrameworkLogger *_logger{nullptr};
        std::unique_ptr<DependencyTrackerRegistration> _loggerTrackerRegistration;
        std::unordered_map<uint64_t, LogT*> _loggers;
    };
}