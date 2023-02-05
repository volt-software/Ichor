#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/dependency_management/Service.h>
#include <ichor/services/logging/Logger.h>

namespace Ichor {
    struct ILoggerFactory {
        virtual void setDefaultLogLevel(LogLevel level) = 0;
        [[nodiscard]] virtual LogLevel getDefaultLogLevel() const = 0;

        protected:
            ~ILoggerFactory() = default;
    };

    template<typename LogT>
    class LoggerFactory final : public ILoggerFactory, public Service<LoggerFactory<LogT>> {
    public:
        LoggerFactory(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service<LoggerFactory<LogT>>(std::move(props), mng) {
            reg.registerDependency<IFrameworkLogger>(this, false);

            auto logLevelProp = Service<LoggerFactory<LogT>>::getProperties().find("DefaultLogLevel");
            if(logLevelProp != end(Service<LoggerFactory<LogT>>::getProperties())) {
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
        AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
            _loggerTrackerRegistration = Service<LoggerFactory<LogT>>::getManager().template registerDependencyTracker<ILogger>(this);
            co_return {};
        }

        AsyncGenerator<void> stop() final {
            _loggerTrackerRegistration.reset();
            co_return;
        }

        void addDependencyInstance(IFrameworkLogger *logger, IService *) noexcept {
            _logger = logger;
        }

        void removeDependencyInstance(IFrameworkLogger *, IService *) noexcept {
            _logger = nullptr;
        }

        void handleDependencyRequest(ILogger *, DependencyRequestEvent const &evt) {
            auto logger = _loggers.find(evt.originatingService);

            if (logger == end(_loggers)) {
                auto requestedLevel = _defaultLevel;
                if(evt.properties.has_value()) {
                    auto requestedLevelIt = evt.properties.value()->find("LogLevel");
                    requestedLevel = requestedLevelIt != end(*evt.properties.value()) ? Ichor::any_cast<LogLevel>(requestedLevelIt->second) : requestedLevel;
                }

                Properties props{};
                props.template emplace<>("Filter", Ichor::make_any<Filter>(Filter{ServiceIdFilterEntry{evt.originatingService}}));
                props.template emplace<>("DefaultLogLevel", Ichor::make_any<LogLevel>(requestedLevel));
                auto *newLogger = Service<LoggerFactory<LogT>>::getManager().template createServiceManager<LogT, ILogger>(std::move(props));
                _loggers.emplace(evt.originatingService, newLogger);
            } else {
                ICHOR_LOG_TRACE(_logger, "svcid {} already has logger", evt.originatingService);
            }
        }

        void handleDependencyUndoRequest(ILogger *, DependencyUndoRequestEvent const &evt) {
            auto service = _loggers.find(evt.originatingService);
            if(service != end(_loggers)) {
                Service<LoggerFactory<LogT>>::getManager().template pushEvent<StopServiceEvent>(Service<LoggerFactory<LogT>>::getServiceId(), service->second->getServiceId());
                // + 11 because the first stop triggers a dep offline event and inserts a new stop with 10 higher priority.
                Service<LoggerFactory<LogT>>::getManager().template pushPrioritisedEvent<RemoveServiceEvent>(Service<LoggerFactory<LogT>>::getServiceId(), INTERNAL_EVENT_PRIORITY + 11, service->second->getServiceId());
                _loggers.erase(service);
            }
        }

        friend DependencyRegister;
        friend DependencyManager;

        IFrameworkLogger *_logger{nullptr};
        DependencyTrackerRegistration _loggerTrackerRegistration{};
        unordered_map<uint64_t, IService*> _loggers;
        LogLevel _defaultLevel{LogLevel::LOG_ERROR};
    };
}
