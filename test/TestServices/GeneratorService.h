#pragma once

#include <ichor/Service.h>

using namespace Ichor;

struct IGeneratorService {
    virtual Generator<int> infinite_int() = 0;
};
struct GeneratorService final : public IGeneratorService, public Service<GeneratorService> {
    GeneratorService() = default;

    Generator<int> infinite_int() final {
        int i{};
        for(;;) {
            co_yield i;
            i++;
        }
    }
};