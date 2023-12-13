#pragma once

#include <vector>
#include <memory>
#include <tuple>

namespace Ichor {

    template <typename T>
    class ISerializer {
    public:
        virtual std::vector<uint8_t> serialize(T const &obj) = 0;
        virtual tl::optional<T> deserialize(std::vector<uint8_t> &&stream) = 0;

    protected:
        ~ISerializer() = default;
    };
}
