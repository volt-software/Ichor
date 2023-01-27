#pragma once

#include "UselessService.h"
#include "DependencyService.h"

using namespace Ichor;
struct IConstructorInjectionTestService {};

struct ConstructorInjectionTestService final : public IConstructorInjectionTestService {
    ConstructorInjectionTestService(ILogger *logSvc, ICountService *countSvc) {
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
    }
};
