#pragma once

#include <ichor/dependency_management/Service.h>

using namespace Ichor;

struct IGeneratorService {
    virtual ~IGeneratorService() = default;
    virtual AsyncGenerator<int> infinite_int() = 0;
};
struct GeneratorService final : public IGeneratorService, public Service<GeneratorService> {
    GeneratorService() = default;
    ~GeneratorService() final = default;

    AsyncGenerator<int> infinite_int() final {
        int i{};
        for(;;) {
            co_yield i;
            i++;
        }
        co_return i;
    }
};
