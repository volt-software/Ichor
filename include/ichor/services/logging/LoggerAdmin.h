#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/dependency_management/Service.h>
#include <ichor/dependency_management/ILifecycleManager.h>
#include <ichor/services/logging/Logger.h>

namespace Ichor {
    struct ILoggerAdmin {
        virtual void setDefaultLogLevel(LogLevel level) = 0;
        [[nodiscard]] virtual LogLevel getDefaultLogLevel() const = 0;

        protected:
            ~ILoggerAdmin() = default;
    };

    template<typename LogT>
    class LoggerAdmin final : public ILoggerAdmin, public Service<LoggerAdmin<LogT>> {
    public:
        LoggerAdmin(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service<LoggerAdmin<LogT>>(std::move(props), mng), _loggers() {
            reg.registerDependency<IFrameworkLogger>(this, false);
        }
        ~LoggerAdmin() final = default;


        void setDefaultLogLevel(LogLevel level) final {
            _defaultLevel = level;
        }

        [[nodiscard]] LogLevel getDefaultLogLevel() const final {
            return _defaultLevel;
        }

    private:
        AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
            _loggerTrackerRegistration = Service<LoggerAdmin<LogT>>::getManager().template registerDependencyTracker<ILogger>(this);
            co_return {};
        }

        AsyncGenerator<void> stop() final {
            _loggerTrackerRegistration.reset();
            co_return;
        }

        void addDependencyInstance(IFrameworkLogger *logger, IService *isvc) noexcept {
            _logger = logger;
        }

        void removeDependencyInstance(IFrameworkLogger *logger, IService *isvc) noexcept {
            _logger = nullptr;
        }

        void handleDependencyRequest(ILogger *, DependencyRequestEvent const &evt) {
            auto logger = _loggers.find(evt.originatingService);

            auto requestedLevel = _defaultLevel;
            if(evt.properties.has_value()) {
                auto requestedLevelIt = evt.properties.value()->find("LogLevel");
                requestedLevel = requestedLevelIt != end(*evt.properties.value()) ? Ichor::any_cast<LogLevel>(requestedLevelIt->second) : requestedLevel;
            }
            if (logger == end(_loggers)) {
                Properties props{};
                props.template emplace<>("Filter", Ichor::make_any<Filter>(Filter{ServiceIdFilterEntry{evt.originatingService}}));
                auto it = _loggers.emplace(evt.originatingService, Service<LoggerAdmin<LogT>>::getManager().template createServiceManager<LogT, ILogger>(std::move(props)));
                it.first->second->setLogLevel(requestedLevel);
            } else {
                ICHOR_LOG_TRACE(_logger, "svcid {} already has logger", evt.originatingService);
            }
        }

        void handleDependencyUndoRequest(ILogger *, DependencyUndoRequestEvent const &evt) {
            auto service = _loggers.find(evt.originatingService);
            if(service != end(_loggers)) {
                Service<LoggerAdmin<LogT>>::getManager().template pushEvent<StopServiceEvent>(Service<LoggerAdmin<LogT>>::getServiceId(), service->second->getServiceId());
                // + 11 because the first stop triggers a dep offline event and inserts a new stop with 10 higher priority.
                Service<LoggerAdmin<LogT>>::getManager().template pushPrioritisedEvent<RemoveServiceEvent>(Service<LoggerAdmin<LogT>>::getServiceId(), INTERNAL_EVENT_PRIORITY + 11, service->second->getServiceId());
                _loggers.erase(service);
            }
        }

        friend DependencyRegister;
        friend DependencyManager;

        IFrameworkLogger *_logger{nullptr};
        DependencyTrackerRegistration _loggerTrackerRegistration{};
        unordered_map<uint64_t, LogT*> _loggers;
        LogLevel _defaultLevel{LogLevel::LOG_ERROR};
    };
}
