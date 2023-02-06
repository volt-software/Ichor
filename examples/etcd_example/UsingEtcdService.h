#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/etcd/IEtcdService.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/ILifecycleManager.h>

using namespace Ichor;

class UsingEtcdService final : public Service<UsingEtcdService> {
public:
    UsingEtcdService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IEtcdService>(this, true);
    }
    ~UsingEtcdService() final = default;

private:
    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
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

        getManager().pushEvent<QuitEvent>(getServiceId());
        co_return {};
    }

    AsyncGenerator<void> stop() final {
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
    friend DependencyManager;

    ILogger *_logger{nullptr};
    IEtcdService *_etcd{nullptr};
};
