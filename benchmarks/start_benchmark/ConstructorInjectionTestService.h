#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/DependencyManager.h>

using namespace Ichor;

class ConstructorInjectionTestService {
public:
    ConstructorInjectionTestService(ILogger *) {
        IService const *self = GetThreadLocalManager().getIServiceForImplementation(this);
        auto iteration = Ichor::any_cast<uint64_t>(self->getProperties().find("Iteration")->second);
        if(iteration == SERVICES_COUNT - 1) {
            GetThreadLocalManager().pushEvent<QuitEvent>(self->getServiceId());
        }
    }
};
