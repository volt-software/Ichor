#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/etcd_bundle/IEtcdService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;

class UsingEtcdService final : public Service<UsingEtcdService> {
public:
    UsingEtcdService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IEtcdService>(this, true);
    }
    ~UsingEtcdService() final = default;

    StartBehaviour start() final {
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

        getManager()->pushEvent<QuitEvent>(getServiceId(), INTERNAL_EVENT_PRIORITY+1);
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        ICHOR_LOG_INFO(_logger, "UsingEtcdService stopped");
        return StartBehaviour::SUCCEEDED;
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

private:
    ILogger *_logger{nullptr};
    IEtcdService *_etcd{nullptr};
};