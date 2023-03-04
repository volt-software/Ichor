#pragma once

#include "UselessService.h"
#include "DependencyService.h"

using namespace Ichor;
struct IConstructorInjectionTestService {
    virtual ~IConstructorInjectionTestService() {}
    [[nodiscard]] virtual uint64_t getServiceId() const noexcept = 0;
};

struct ConstructorInjectionTestService final : public IConstructorInjectionTestService {
    ConstructorInjectionTestService(IEventQueue *q, ILogger *logSvc, ICountService *countSvc) {
        if(q == nullptr) {
            std::terminate();
        }
        if(logSvc == nullptr) {
            std::terminate();
        }
        if(countSvc == nullptr) {
            std::terminate();
        }
        ICHOR_LOG_INFO(logSvc, "test");

        if(!countSvc->isRunning()) {
            std::terminate();
        }

        auto *self = GetThreadLocalManager().getIServiceForImplementation(this);
        if(self == nullptr) {
            std::terminate();
        }
        _serviceId = self->getServiceId();
    }

    [[nodiscard]] uint64_t getServiceId() const noexcept final {
        return _serviceId;
    }

    uint64_t _serviceId{};
};
