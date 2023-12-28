#pragma once

#include "UselessService.h"
#include <ichor/dependency_management/DependencyRegister.h>

using namespace Ichor;

struct INotUsed {

};

struct MultipleSeparateDependencyRequestsService final : public AdvancedService<MultipleSeparateDependencyRequestsService> {
    MultipleSeparateDependencyRequestsService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<INotUsed>(this, false, Properties{{"scope", Ichor::make_any<std::string>("scope_one")}});
        reg.registerDependency<IUselessService>(this, true, Properties{{"scope", Ichor::make_any<std::string>("scope_one")}});
        reg.registerDependency<Ichor::IUselessService>(this, true, Properties{{"scope", Ichor::make_any<std::string>("scope_two")}});
    }
    ~MultipleSeparateDependencyRequestsService() final = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        fmt::print("multiple start\n");
        if(_depCount == 2) {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        }
        co_return {};
    }

    void addDependencyInstance(IUselessService&, IService&) {
        _depCount++;

        fmt::print("multiple requests: {}\n", _depCount);
    }

    void removeDependencyInstance(IUselessService&, IService&) {
    }

    void addDependencyInstance(INotUsed&, IService&) {
        throw std::runtime_error("INotUsed should never be injected");
    }

    void removeDependencyInstance(INotUsed&, IService&) {
        throw std::runtime_error("INotUsed should never be injected");
    }

    uint64_t _depCount{};
};
