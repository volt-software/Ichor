#pragma once

#include <variant>
#include <ichor/Service.h>
#include "DependencyService.h"

using namespace Ichor;

struct IMixOne {
    virtual uint32_t one() = 0;
};
struct IMixTwo {
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

struct Handle {
    IService *isvc{};
    std::variant<IMixOne*, IMixTwo*> iface{};
};

struct CheckMixService final : public ICountService, public Service<CheckMixService> {
    CheckMixService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service<CheckMixService>(std::move(props), mng) {
        reg.registerDependency<IMixOne>(this, false);
        reg.registerDependency<IMixTwo>(this, false);
    }

    static void check(IMixOne *one, IMixTwo *two) {
        REQUIRE(one->one() != two->two());
    }

    void remove(IService *svc) {
        svcCount--;

        _services.erase(svc->getServiceId());
    }

    void addDependencyInstance(IMixOne *svc, IService *isvc) {
        svcCount++;

        auto existing = _services.find(isvc->getServiceId());

        if(existing != end(_services)) {
            REQUIRE(existing->second.isvc->getServiceId() == isvc->getServiceId());
            REQUIRE(existing->second.isvc->getServicePriority() == isvc->getServicePriority());
            REQUIRE(existing->second.isvc == isvc);

            auto *existingMixTwo = std::get<IMixTwo*>(existing->second.iface);

            check(svc, existingMixTwo);
        } else {
            _services.emplace(isvc->getServiceId(), Handle{isvc, svc});
        }

        if(svcCount == 12) {
            getManager()->pushEvent<QuitEvent>(0);
        }
    }

    void removeDependencyInstance(IMixOne *, IService *isvc) {
        remove(isvc);
    }

    void addDependencyInstance(IMixTwo *svc, IService *isvc) {
        svcCount++;

        auto existing = _services.find(isvc->getServiceId());

        if(existing != end(_services)) {
            REQUIRE(existing->second.isvc->getServiceId() == isvc->getServiceId());
            REQUIRE(existing->second.isvc->getServicePriority() == isvc->getServicePriority());
            REQUIRE(existing->second.isvc == isvc);

            auto *existingMixOne = std::get<IMixOne*>(existing->second.iface);

            check(existingMixOne, svc);
        } else {
            _services.emplace(isvc->getServiceId(), Handle{isvc, svc});
        }

        if(svcCount == 12) {
            getManager()->pushEvent<QuitEvent>(0);
        }
    }

    void removeDependencyInstance(IMixTwo *, IService *isvc) {
        remove(isvc);
    }

    [[nodiscard]] uint64_t getSvcCount() const noexcept final {
        return svcCount;
    }

    uint64_t svcCount{};
    std::unordered_map<uint64_t, Handle> _services;
};