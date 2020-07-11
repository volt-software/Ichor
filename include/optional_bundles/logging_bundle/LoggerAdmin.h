#pragma once

#include <framework/DependencyManager.h>
#include "framework/Service.h"
#include "framework/ServiceLifecycleManager.h"
#include "Logger.h"


struct ILoggerAdmin : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

template <typename T>
class LoggerAdmin : public ILoggerAdmin, public Service {
public:
    ~LoggerAdmin() final = default;

    bool start() final {
        LOG_INFO(_logger, "LoggerAdmin started");
        _loggerTrackerRegistration = _manager->registerDependencyTracker<ILogger>(getServiceId(), this);
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "LoggerAdmin stopped");
        return true;
    }

    void addDependencyInstance(IFrameworkLogger *logger) noexcept {
        _logger = logger;
        LOG_INFO(_logger, "Inserted logger");
    }

    void removeDependencyInstance(IFrameworkLogger *logger) noexcept {
        _logger = nullptr;
    }

    void handleDependencyRequest(ILogger*, DependencyRequestEvent const * const evt) {
        auto logger = _loggers.find(evt->originatingService);

        auto requestedLevelIt = evt->properties->find("LogLevel");
        auto requestedLevel = requestedLevelIt != end(*evt->properties) ? std::any_cast<LogLevel>(requestedLevelIt->second) : LogLevel::INFO;
        if(logger == end(_loggers)) {
            LOG_INFO(_logger, "creating logger for svcid {}", evt->originatingService);
            _loggers.emplace(evt->originatingService, _manager->createServiceManager<ILogger, T>(CppelixProperties{{"LogLevel", requestedLevel}, {"TargetServiceId", evt->originatingService}, {"Filter", Filter{ServiceIdFilterEntry{evt->originatingService}}}}));
        } else {
            LOG_INFO(_logger, "svcid {} already has logger", evt->originatingService);
        }
    }

    void handleDependencyUndoRequest(ILogger*, DependencyUndoRequestEvent const * const evt) {
        _loggers.erase(evt->originatingService);
    }

private:
    IFrameworkLogger *_logger;
    std::unique_ptr<DependencyTrackerRegistration> _loggerTrackerRegistration;
    std::unordered_map<uint64_t, T*> _loggers;
};