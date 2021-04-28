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

    bool start() final {
        ICHOR_LOG_INFO(_logger, "UsingEtcdService started");
        auto start = std::chrono::steady_clock::now();
        for(uint64_t i = 0; i < 10'000; i++) {
            if(!_etcd->put("test" + std::to_string(i), "2") || !_etcd->get("test" + std::to_string(i))) {
                throw std::runtime_error("Couldn't put or get");
            }

            if(i % 1'000 == 0) {
                ICHOR_LOG_INFO(_logger, "iteration {}", i);
            }
        }
        auto end = std::chrono::steady_clock::now();
        ICHOR_LOG_INFO(_logger, "finished in {:L} µs", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());
        getManager()->pushEvent<QuitEvent>(getServiceId(), INTERNAL_EVENT_PRIORITY+1);
        return true;
    }

    bool stop() final {
        ICHOR_LOG_INFO(_logger, "UsingEtcdService stopped");
        return true;
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