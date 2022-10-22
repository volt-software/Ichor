#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>

namespace Ichor {
    struct ILoggerAdmin {
    };

    template<typename LogT>
    class LoggerAdmin final : public ILoggerAdmin, public Service<LoggerAdmin<LogT>> {
    public:
        LoggerAdmin(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service<LoggerAdmin<LogT>>(std::move(props), mng), _loggers() {
            reg.registerDependency<IFrameworkLogger>(this, true);
        }
        ~LoggerAdmin() final = default;

        StartBehaviour start() final {
            _loggerTrackerRegistration = Service<LoggerAdmin<LogT>>::getManager()->template registerDependencyTracker<ILogger>(this);
            return StartBehaviour::SUCCEEDED;
        }

        StartBehaviour stop() final {
            _loggerTrackerRegistration.reset();
            return StartBehaviour::SUCCEEDED;
        }

        void addDependencyInstance(IFrameworkLogger *logger, IService *isvc) noexcept {
            _logger = logger;
        }

        void removeDependencyInstance(IFrameworkLogger *logger, IService *isvc) noexcept {
            _logger = nullptr;
        }

        void handleDependencyRequest(ILogger *, DependencyRequestEvent const &evt) {
            auto logger = _loggers.find(evt.originatingService);

//            ICHOR_LOG_ERROR(_logger, "dep req {} dm {}", evt.originatingService, getManager()->getId());

            auto requestedLevel = LogLevel::INFO;
            if(evt.properties.has_value()) {
                auto requestedLevelIt = evt.properties.value()->find("LogLevel");
                requestedLevel = requestedLevelIt != end(*evt.properties.value()) ? Ichor::any_cast<LogLevel>(requestedLevelIt->second) : LogLevel::INFO;
            }
            if (logger == end(_loggers)) {
//                ICHOR_LOG_ERROR(_logger, "creating logger for svcid {}", evt.originatingService);
                    Properties props{};
                    props.reserve(3);
                    props.template emplace("LogLevel",        Ichor::make_any<LogLevel>(requestedLevel));
                    props.template emplace("TargetServiceId", Ichor::make_any<uint64_t>(evt.originatingService));
                    props.template emplace("Filter",          Ichor::make_any<Filter>(Filter{ServiceIdFilterEntry{evt.originatingService}}));
                    _loggers.emplace(evt.originatingService, Service<LoggerAdmin<LogT>>::getManager()->template createServiceManager<LogT, ILogger>(std::move(props)));
            } else {
                ICHOR_LOG_TRACE(_logger, "svcid {} already has logger", evt.originatingService);
            }
        }

        void handleDependencyUndoRequest(ILogger *, DependencyUndoRequestEvent const &evt) {
            _loggers.erase(evt.originatingService);
        }

    private:
        IFrameworkLogger *_logger{nullptr};
        DependencyTrackerRegistration _loggerTrackerRegistration{};
        std::unordered_map<uint64_t, LogT*> _loggers;
    };
}