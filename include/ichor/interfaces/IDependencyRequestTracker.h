#pragma once

#include <vector>
#include <memory>

namespace Ichor {

    class IDependencyRequestTracker {
    public:
        virtual ~IDependencyRequestTracker() = default;

        virtual void handleDependencyRequest(void * ) = 0;
        virtual void* deserialize(const std::vector<uint8_t> &stream) = 0;
    };
}