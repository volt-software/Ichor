#pragma once

#include <ichor/Service.h>
#include "DependencyService.h"

using namespace Ichor;

struct IMixOne : virtual public IService {
    virtual uint32_t one() = 0;
};
struct IMixTwo : virtual public IService {
    virtual uint32_t two() = 0;
};
struct MixServiceOne final : public IMixOne, public IMixTwo, public Service<MixServiceOne> {
    uint32_t one() final { return 1; }
    uint32_t two() final { return 2; }
};
struct MixServiceTwo final : public IMixTwo, public IMixOne, public Service<MixServiceTwo> {
    uint32_t one() final { return 3; }
    uint32_t two() final { return 4; }
};
struct MixServiceThree final : public IMixTwo, public Service<MixServiceThree>, public IMixOne {
    uint32_t one() final { return 5; }
    uint32_t two() final { return 6; }
};
struct MixServiceFour final : public Service<MixServiceFour>, public IMixTwo, public IMixOne {
    uint32_t one() final { return 7; }
    uint32_t two() final { return 8; }
};
struct MixServiceFive final : public Service<MixServiceFive>, public IMixOne, public IMixTwo {
    uint32_t one() final { return 9; }
    uint32_t two() final { return 10; }
};
struct MixServiceSix final : public IMixOne, public Service<MixServiceSix>, public IMixTwo {
    uint32_t one() final { return 11; }
    uint32_t two() final { return 12; }
};

struct CheckMixService final : public ICountService, public Service<CheckMixService> {
    CheckMixService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service<CheckMixService>(std::move(props), mng) {
        reg.registerDependency<IMixOne>(this, false);
        reg.registerDependency<IMixTwo>(this, false);
    }

    void check(IMixOne *one, IMixTwo *two) {
        REQUIRE(one->getServiceId() == two->getServiceId());
        REQUIRE(one->getServicePriority() == two->getServicePriority());
        REQUIRE(one->one() != two->two());
    }

    void add(IService *svc) {
        svcCount++;

        auto existing = _services.find(svc->getServiceId());

        if(existing != end(_services)) {
            REQUIRE(existing->second->getServiceId() == svc->getServiceId());
            REQUIRE(existing->second->getServicePriority() == svc->getServicePriority());
            REQUIRE(existing->second == svc);

            IMixOne *newone = reinterpret_cast<IMixOne*>(svc);
            IMixTwo *newtwo = reinterpret_cast<IMixTwo*>(svc);
            IMixOne *existingone = reinterpret_cast<IMixOne*>(svc);
            IMixTwo *existingtwo = reinterpret_cast<IMixTwo*>(svc);

            check(newone, newtwo);
            check(existingone, existingtwo);
            check(newone, existingtwo);
            check(existingone, newtwo);
        } else {
            _services.emplace(svc->getServiceId(), svc);
        }

        if(svcCount == 12) {
            getManager()->pushEvent<QuitEvent>(0);
        }
    }

    void remove(IService *svc) {
        svcCount--;

        _services.erase(svc->getServiceId());
    }

    void addDependencyInstance(IMixOne *svc) {
        add(svc);
    }

    void removeDependencyInstance(IMixOne *svc) {
        remove(svc);
    }

    void addDependencyInstance(IMixTwo *svc) {
        add(svc);
    }

    void removeDependencyInstance(IMixTwo *svc) {
        remove(svc);
    }

    [[nodiscard]] uint64_t getSvcCount() const noexcept final {
        return svcCount;
    }

    uint64_t svcCount{};
    std::unordered_map<uint64_t, IService*> _services;
};