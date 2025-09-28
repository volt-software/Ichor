#pragma once

#include <variant>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/ServiceExecutionScope.h>

using namespace Ichor;

struct IMixOne {
    virtual ~IMixOne() = default;
    virtual uint32_t one() = 0;
};
struct IMixTwo {
    virtual ~IMixTwo() = default;
    virtual uint32_t two() = 0;
};
struct MixServiceOne final : public IMixOne, public IMixTwo, public AdvancedService<MixServiceOne> {
    MixServiceOne() = default;
    ~MixServiceOne() final = default;
    uint32_t one() final { return 1; }
    uint32_t two() final { return 2; }
};
struct MixServiceTwo final : public IMixTwo, public IMixOne, public AdvancedService<MixServiceTwo> {
    MixServiceTwo() = default;
    ~MixServiceTwo() final = default;
    uint32_t one() final { return 3; }
    uint32_t two() final { return 4; }
};
struct MixServiceThree final : public IMixTwo, public AdvancedService<MixServiceThree>, public IMixOne {
    MixServiceThree() = default;
    ~MixServiceThree() final = default;
    uint32_t one() final { return 5; }
    uint32_t two() final { return 6; }
};
struct MixServiceFour final : public AdvancedService<MixServiceFour>, public IMixTwo, public IMixOne {
    MixServiceFour() = default;
    ~MixServiceFour() final = default;
    uint32_t one() final { return 7; }
    uint32_t two() final { return 8; }
};
struct MixServiceFive final : public AdvancedService<MixServiceFive>, public IMixOne, public IMixTwo {
    MixServiceFive() = default;
    ~MixServiceFive() final = default;
    uint32_t one() final { return 9; }
    uint32_t two() final { return 10; }
};
struct MixServiceSix final : public IMixOne, public AdvancedService<MixServiceSix>, public IMixTwo {
    MixServiceSix() = default;
    ~MixServiceSix() final = default;
    uint32_t one() final { return 11; }
    uint32_t two() final { return 12; }
};

struct Handle {
    IService *isvc{};
    std::variant<Ichor::ScopedServiceProxy<IMixOne*>, Ichor::ScopedServiceProxy<IMixTwo*>> iface{};
};

struct CheckMixService final : public ICountService, public AdvancedService<CheckMixService> {
    CheckMixService(DependencyRegister &reg, Properties props) : AdvancedService<CheckMixService>(std::move(props)) {
        reg.registerDependency<IMixOne>(this, DependencyFlags::ALLOW_MULTIPLE);
        reg.registerDependency<IMixTwo>(this, DependencyFlags::ALLOW_MULTIPLE);
    }

    static void check(Ichor::ScopedServiceProxy<IMixOne*> &one, Ichor::ScopedServiceProxy<IMixTwo*> &two) {
        REQUIRE(one->one() != two->two());
    }

    void remove(IService &svc) {
        svcCount--;

        _services.erase(svc.getServiceId());
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<IMixOne*> svc, IService &isvc) {
        svcCount++;

        auto existing = _services.find(isvc.getServiceId());

        if(existing != end(_services)) {
            REQUIRE(existing->second.isvc->getServiceId() == isvc.getServiceId());
            REQUIRE(existing->second.isvc->getServicePriority() == isvc.getServicePriority());
            REQUIRE(existing->second.isvc == &isvc);

            auto existingMixTwo = std::get<Ichor::ScopedServiceProxy<IMixTwo*>>(existing->second.iface);

            check(svc, existingMixTwo);
        } else {
            _services.emplace(isvc.getServiceId(), Handle{&isvc, svc});
        }

        if(svcCount == 12) {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(0);
        }
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<IMixOne*>, IService &isvc) {
        remove(isvc);
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<IMixTwo*> svc, IService &isvc) {
        svcCount++;

        auto existing = _services.find(isvc.getServiceId());

        if(existing != end(_services)) {
            REQUIRE(existing->second.isvc->getServiceId() == isvc.getServiceId());
            REQUIRE(existing->second.isvc->getServicePriority() == isvc.getServicePriority());
            REQUIRE(existing->second.isvc == &isvc);

            auto existingMixOne = std::get<Ichor::ScopedServiceProxy<IMixOne*>>(existing->second.iface);

            check(existingMixOne, svc);
        } else {
            _services.emplace(isvc.getServiceId(), Handle{&isvc, svc});
        }

        if(svcCount == 12) {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(0);
        }
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<IMixTwo*>, IService &isvc) {
        remove(isvc);
    }

    [[nodiscard]] uint64_t getSvcCount() const noexcept final {
        return svcCount;
    }

    [[nodiscard]] bool isRunning() const noexcept final {
        return true;
    }

    uint64_t svcCount{};
    std::unordered_map<uint64_t, Handle> _services;
};
