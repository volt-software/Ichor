#pragma once

#include <framework/DependencyManager.h>
#include <optional_bundles/logging_bundle/Logger.h>
#include <optional_bundles/etcd_bundle/IEtcdService.h>
#include "framework/Service.h"
#include "framework/LifecycleManager.h"

using namespace Cppelix;


struct IUsingEtcdService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class UsingEtcdService final : public IUsingEtcdService, public Service {
public:
    ~UsingEtcdService() final = default;

    bool start() final {
        LOG_INFO(_logger, "UsingEtcdService started");
        auto start = std::chrono::system_clock::now();
        for(uint64_t i = 0; i < 100'000; i++) {
            if(!_etcd->put("test" + std::to_string(i), "2") || !_etcd->get("test" + std::to_string(i))) {
                throw std::runtime_error("Couldn't put or get");
            }

            if(i % 1'000 == 0) {
                LOG_INFO(_logger, "iteration {}", i);
            }
        }
        auto end = std::chrono::system_clock::now();
        LOG_INFO(_logger, "finished in {:L} µs", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());
        getManager()->pushEvent<QuitEvent>(getServiceId(), INTERNAL_EVENT_PRIORITY+1);
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "UsingEtcdService stopped");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(IEtcdService *etcd) {
        _etcd = etcd;
    }

    void removeDependencyInstance(IEtcdService *etcd) {
        _etcd = nullptr;
    }

private:
    ILogger *_logger{nullptr};
    IEtcdService *_etcd{nullptr};
};