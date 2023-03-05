#pragma once

#include "UselessService.h"
#include "DependencyService.h"

using namespace Ichor;
struct IConstructorInjectionTestService {
    virtual ~IConstructorInjectionTestService() {}
    [[nodiscard]] virtual uint64_t getServiceId() const noexcept = 0;
};

struct ConstructorInjectionTestService final : public IConstructorInjectionTestService {
    ConstructorInjectionTestService(IEventQueue *q, ILogger *logSvc, ICountService *countSvc, IService *self) {
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

    [[nodiscard]] uint64_t getServiceId() const noexcept final {
        return _serviceId;
    }

    uint64_t _serviceId{};
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
        q->pushEvent<QuitEvent>(0);
    }
};

struct ConstructorInjectionQuitService3 final {
    ConstructorInjectionQuitService3(DependencyManager *dm) {
        if(dm == nullptr) {
            std::terminate();
        }
        dm->getEventQueue().pushEvent<QuitEvent>(0);
    }
};

struct ConstructorInjectionQuitService4 final {
    ConstructorInjectionQuitService4(IService *self) {
        if(self == nullptr) {
            std::terminate();
        }
    }
};
