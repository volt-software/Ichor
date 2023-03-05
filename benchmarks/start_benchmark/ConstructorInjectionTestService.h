#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/DependencyManager.h>

using namespace Ichor;

class ConstructorInjectionTestService {
public:
    ConstructorInjectionTestService(ILogger *, DependencyManager *dm, IService *self) {
        auto iteration = Ichor::any_cast<uint64_t>(self->getProperties().find("Iteration")->second);
        if(iteration == SERVICES_COUNT - 1) {
            dm->getEventQueue().pushEvent<QuitEvent>(self->getServiceId());
        }
    }
};
