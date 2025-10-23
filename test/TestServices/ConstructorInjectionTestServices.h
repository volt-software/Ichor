#pragma once

#include "UselessService.h"

using namespace Ichor;
using namespace Ichor::v1;

struct IConstructorInjectionTestService {
    virtual ~IConstructorInjectionTestService() {}
    [[nodiscard]] virtual ServiceIdType getServiceId() const noexcept = 0;
};

struct ConstructorInjectionTestService final : public IConstructorInjectionTestService {
    ConstructorInjectionTestService(IEventQueue *q, ScopedServiceProxy<ILogger> logSvc, ScopedServiceProxy<ICountService> countSvc, IService *self) {
        if(q == nullptr) {
            std::terminate();
        }
        if(logSvc == nullptr) {
            std::terminate();
        }
        if(countSvc == nullptr) {
            std::terminate();
        }
        if(self == nullptr) {
            std::terminate();
        }
        ICHOR_LOG_INFO(logSvc, "test");

        if(!countSvc->isRunning()) {
            std::terminate();
        }

        _serviceId = self->getServiceId();
    }

    [[nodiscard]] ServiceIdType getServiceId() const noexcept final {
        return _serviceId;
    }

    ServiceIdType _serviceId{};
};

struct ConstructorInjectionQuitService final {
    ConstructorInjectionQuitService(IEventQueue *q, DependencyManager *dm, IService *self) {
        if(q == nullptr) {
            std::terminate();
        }
        if(dm == nullptr) {
            std::terminate();
        }
        if(self == nullptr) {
            std::terminate();
        }
        q->pushEvent<QuitEvent>(self->getServiceId());
    }
};

struct ConstructorInjectionQuitService2 final {
    ConstructorInjectionQuitService2(IEventQueue *q) {
        if(q == nullptr) {
            std::terminate();
        }
        q->pushEvent<QuitEvent>(ServiceIdType{0});
    }
};

struct ConstructorInjectionQuitService3 final {
    ConstructorInjectionQuitService3(DependencyManager *dm) {
        if(dm == nullptr) {
            std::terminate();
        }
        dm->getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
    }
};

struct ConstructorInjectionQuitService4 final {
    ConstructorInjectionQuitService4(IService *self) {
        if(self == nullptr) {
            std::terminate();
        }
    }
};
