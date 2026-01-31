#pragma once

#include "UselessService.h"
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/ScopedServiceProxy.h>

using namespace Ichor;
using namespace Ichor::v1;

struct INotUsed {

};

struct MultipleSeparateDependencyRequestsService final : public AdvancedService<MultipleSeparateDependencyRequestsService> {
    MultipleSeparateDependencyRequestsService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<INotUsed>(this, DependencyFlags::NONE, Properties{{"scope", Ichor::v1::make_any<std::string>("scope_one")}});
        reg.registerDependency<IUselessService>(this, DependencyFlags::REQUIRED, Properties{{"scope", Ichor::v1::make_any<std::string>("scope_one")}});
        reg.registerDependency<Ichor::IUselessService>(this, DependencyFlags::REQUIRED, Properties{{"scope", Ichor::v1::make_any<std::string>("scope_two")}});
    }
    ~MultipleSeparateDependencyRequestsService() final = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        fmt::print("multiple start\n");
        if(_depCount == 2) {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        }
        co_return {};
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<IUselessService*>, IService&) {
        _depCount++;

        fmt::print("multiple requests: {}\n", _depCount);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<IUselessService*>, IService&) {
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<INotUsed*>, IService&) {
        std::terminate();
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<INotUsed*>, IService&) {
        std::terminate();
    }

    uint64_t _depCount{};
};
