#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/services/etcd/IEtcdService.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/event_queues/IEventQueue.h>

using namespace Ichor;

class UsingEtcdService final : public AdvancedService<UsingEtcdService> {
public:
    UsingEtcdService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IEtcdService>(this, true);
    }
    ~UsingEtcdService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "UsingEtcdService started");
        if(_etcd->put("test", "2")) {
            ICHOR_LOG_TRACE(_logger, "Succesfully put key/value into etcd");
            auto storedVal = _etcd->get("test");
            if(storedVal == "2") {
                ICHOR_LOG_TRACE(_logger, "Succesfully retrieved key/value into etcd");
            } else {
                ICHOR_LOG_ERROR(_logger, "Error retrieving key/value into etcd");
            }
        } else {
            ICHOR_LOG_ERROR(_logger, "Error putting key/value into etcd");
        }

        GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "UsingEtcdService stopped");
        co_return;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    void addDependencyInstance(IEtcdService *etcd, IService *) {
        _etcd = etcd;
    }

    void removeDependencyInstance(IEtcdService *etcd, IService *) {
        _etcd = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger{nullptr};
    IEtcdService *_etcd{nullptr};
};
