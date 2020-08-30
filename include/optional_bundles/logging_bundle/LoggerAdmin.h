#pragma once

#include <framework/DependencyManager.h>
#include "framework/Service.h"
#include "framework/LifecycleManager.h"
#include "Logger.h"

namespace Cppelix {
    struct ILoggerAdmin : virtual public IService {
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
    };

    template<typename LogT>
    class LoggerAdmin final : public ILoggerAdmin, public Service {
    public:
        LoggerAdmin(DependencyRegister &reg, CppelixProperties props) : Service(std::move(props)) {
            reg.registerDependency<IFrameworkLogger>(this, true);
        }
        ~LoggerAdmin() final = default;

        bool start() final {
            _loggerTrackerRegistration = getManager()->template registerDependencyTracker<ILogger>(getServiceId(), this);
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

            auto requestedLevel = LogLevel::INFO;
            if(evt->properties.has_value()) {
                auto requestedLevelIt = evt->properties.value()->find("LogLevel");
                requestedLevel = requestedLevelIt != end(*evt->properties.value()) ? std::any_cast<LogLevel>(requestedLevelIt->second) : LogLevel::INFO;
            }
            if (logger == end(_loggers)) {
                LOG_TRACE(_logger, "creating logger for svcid {}", evt->originatingService);
                    _loggers.emplace(evt->originatingService, getManager()->template createServiceManager<LogT, ILogger>(
                            CppelixProperties{{"LogLevel",        requestedLevel},
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