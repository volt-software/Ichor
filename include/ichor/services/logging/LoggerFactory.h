#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/logging/Logger.h>

namespace Ichor {
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
            reg.registerDependency<IFrameworkLogger>(this, DependencyFlags::NONE);

            auto logLevelProp = AdvancedService<LoggerFactory<LogT>>::getProperties().find("DefaultLogLevel");
            if(logLevelProp != end(AdvancedService<LoggerFactory<LogT>>::getProperties())) {
                setDefaultLogLevel(Ichor::any_cast<LogLevel>(logLevelProp->second));
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

        void addDependencyInstance(IFrameworkLogger &logger, IService&) noexcept {
            _logger = &logger;
        }

        void removeDependencyInstance(IFrameworkLogger&, IService&) noexcept {
            _logger = nullptr;
        }

        AsyncGenerator<IchorBehaviour> handleDependencyRequest(AlwaysNull<ILogger*>, DependencyRequestEvent const &evt) {
            auto logger = _loggers.find(evt.originatingService);

            if (logger == end(_loggers)) {
                auto requestedLevel = _defaultLevel;
                if(evt.properties.has_value()) {
                    auto requestedLevelIt = evt.properties.value()->find("LogLevel");
                    requestedLevel = requestedLevelIt != end(*evt.properties.value()) ? Ichor::any_cast<LogLevel>(requestedLevelIt->second) : requestedLevel;
                }

                Properties props{};
                props.template emplace<>("Filter", Ichor::make_any<Filter>(ServiceIdFilterEntry{evt.originatingService}));
                props.template emplace<>("LogLevel", Ichor::make_any<LogLevel>(requestedLevel));
                auto newLogger = GetThreadLocalManager().template createServiceManager<LogT, ILogger>(std::move(props), evt.priority);
                _loggers.emplace(evt.originatingService, newLogger->getServiceId());
            } else {
                ICHOR_LOG_TRACE(_logger, "svcid {} already has logger", evt.originatingService);
            }

            co_return {};
        }

        AsyncGenerator<IchorBehaviour> handleDependencyUndoRequest(AlwaysNull<ILogger*>, DependencyUndoRequestEvent const &evt) {
            auto service = _loggers.find(evt.originatingService);

            if(service != end(_loggers)) {
                GetThreadLocalEventQueue().template pushPrioritisedEvent<StopServiceEvent>(AdvancedService<LoggerFactory<LogT>>::getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, service->second, true);
                _loggers.erase(service);
            }

            co_return {};
        }

        friend DependencyRegister;
        friend DependencyManager;

        IFrameworkLogger *_logger{};
        DependencyTrackerRegistration _loggerTrackerRegistration{};
        unordered_map<ServiceIdType, ServiceIdType> _loggers;
        LogLevel _defaultLevel{LogLevel::LOG_ERROR};
    };
}
