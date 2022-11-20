#pragma once

#include "UselessService.h"

using namespace Ichor;

struct RegistrationCheckerService final : public Service<RegistrationCheckerService> {
    RegistrationCheckerService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        bool threwException{};

        reg.registerDependency<IUselessService>(this, false);

        try {
            reg.registerDependency<IUselessService>(this, false);
        } catch (const std::runtime_error &e) {
            threwException = true;
        }

        if(!threwException) {
            throw std::runtime_error("Should have thrown exception");
        }

        threwException = false;

        try {
            reg.registerDependency<Ichor::IUselessService>(this, false);
        } catch (const std::runtime_error &e) {
            threwException = true;
        }

        if(!threwException) {
            throw std::runtime_error("Should have thrown exception");
        }

        _executedTests = true;
    }
    ~RegistrationCheckerService() final = default;

    AsyncGenerator<void> start() final {
        if(!_executedTests) {
            throw std::runtime_error("Should have executed tests");
        }
        if(_depCount == 2) {
            getManager().pushEvent<QuitEvent>(getServiceId());
        }
        co_return;
    }

    void addDependencyInstance(IUselessService *, IService *) {
        _depCount++;

        if(_depCount == 2) {
            getManager().pushEvent<QuitEvent>(getServiceId());
        }
    }

    void removeDependencyInstance(IUselessService *, IService *) {
    }

    bool _executedTests{};
    uint64_t _depCount{};
};