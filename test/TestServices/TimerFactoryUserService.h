#pragma once

#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/ScopedServiceProxy.h>

using namespace Ichor;
using namespace Ichor::v1;

/// Gives tests access to the timer factory of a service
class ITimerFactoryUserService {
public:
    [[nodiscard]] virtual ITimerFactory& getTimerFactory() noexcept = 0;
protected:
    ~ITimerFactoryUserService() = default;
};

class TimerFactoryUserService final : public ITimerFactoryUserService, public AdvancedService<TimerFactoryUserService> {
public:
    TimerFactoryUserService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ITimerFactory>(this, DependencyFlags::REQUIRED);
    }
    ~TimerFactoryUserService() final = default;

    [[nodiscard]] ITimerFactory& getTimerFactory() noexcept final {
        return **_timerFactory;
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> factory, IService &) {
        _timerFactory = std::move(factory);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*>, IService&) {
        _timerFactory.reset();
    }

private:
    Ichor::ScopedServiceProxy<ITimerFactory*> _timerFactory {};
};
