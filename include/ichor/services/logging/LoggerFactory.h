#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/Filter.h>
#include <ichor/ScopedServiceProxy.h>

namespace Ichor::v1 {
    struct ILoggerFactory {
        virtual void setDefaultLogLevel(LogLevel level) = 0;
        [[nodiscard]] virtual LogLevel getDefaultLogLevel() const = 0;

        protected:
            ~ILoggerFactory() = default;
    };

    template<typename LogT>
    class LoggerFactory final : public ILoggerFactory, public AdvancedService<LoggerFactory<LogT>> {
    public:
        LoggerFactory(DependencyRegister &reg, Properties props) : AdvancedService<LoggerFactory<LogT>>(std::move(props)) {
            reg.registerDependency<IFrameworkLogger>(this, DependencyFlags::REQUIRED);

            auto logLevelProp = AdvancedService<LoggerFactory<LogT>>::getProperties().find("DefaultLogLevel");
            if(logLevelProp != end(AdvancedService<LoggerFactory<LogT>>::getProperties())) {
                setDefaultLogLevel(Ichor::v1::any_cast<LogLevel>(logLevelProp->second));
            }
        }
        ~LoggerFactory() final = default;

        void setDefaultLogLevel(LogLevel level) final {
            _defaultLevel = level;
        }

        [[nodiscard]] LogLevel getDefaultLogLevel() const final {
            return _defaultLevel;
        }

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final {
            _loggerTrackerRegistration = GetThreadLocalManager().template registerDependencyTracker<ILogger>(this, this);
            co_return {};
        }

        Task<void> stop() final {
            _loggerTrackerRegistration.reset();
            co_return;
        }

        void addDependencyInstance(Ichor::ScopedServiceProxy<IFrameworkLogger*> logger, IService&) noexcept {
            _logger = std::move(logger);
        }

        void removeDependencyInstance(Ichor::ScopedServiceProxy<IFrameworkLogger*>, IService&) noexcept {
            _logger.reset();
        }

        AsyncGenerator<IchorBehaviour> handleDependencyRequest(v1::AlwaysNull<ILogger*>, DependencyRequestEvent const &evt) {
            auto logger = _loggers.find(evt.originatingService);

            if (logger == end(_loggers)) {
                auto requestedLevel = _defaultLevel;
                if(evt.properties.has_value()) {
                    auto requestedLevelIt = evt.properties.value()->find("LogLevel");
                    requestedLevel = requestedLevelIt != end(*evt.properties.value()) ? Ichor::v1::any_cast<LogLevel>(requestedLevelIt->second) : requestedLevel;
                }

                Properties props{};
                props.reserve(2);
                props.template emplace<>("Filter", Ichor::v1::make_any<Filter>(ServiceIdFilterEntry{evt.originatingService}));
                props.template emplace<>("LogLevel", Ichor::v1::make_any<LogLevel>(requestedLevel));
                auto newLogger = GetThreadLocalManager().template createServiceManager<LogT, ILogger>(std::move(props), evt.priority);
                _loggers.emplace(evt.originatingService, newLogger->getServiceId());
                ICHOR_LOG_TRACE(_logger, "created logger for svcid {}", evt.originatingService);
            } else {
                ICHOR_LOG_TRACE(_logger, "svcid {} already has logger", evt.originatingService);
            }

            co_return {};
        }

        AsyncGenerator<IchorBehaviour> handleDependencyUndoRequest(v1::AlwaysNull<ILogger*>, DependencyUndoRequestEvent const &evt) {
            auto service = _loggers.find(evt.originatingService);

            ICHOR_LOG_ERROR(_logger, "svcid {} removing {}", evt.originatingService, service == end(_loggers));

            if(service != end(_loggers)) {
                GetThreadLocalEventQueue().template pushPrioritisedEvent<StopServiceEvent>(AdvancedService<LoggerFactory<LogT>>::getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, service->second, true);
                _loggers.erase(service);
            }

            co_return {};
        }

        friend DependencyRegister;
        friend DependencyManager;

        Ichor::ScopedServiceProxy<IFrameworkLogger*> _logger {};
        DependencyTrackerRegistration _loggerTrackerRegistration{};
        unordered_map<ServiceIdType, ServiceIdType, ServiceIdHash> _loggers;
        LogLevel _defaultLevel{LogLevel::LOG_ERROR};
    };
}
