#pragma once

#include <vector>
#include <memory>
#include <tuple>

namespace Ichor {

    class ISerializer {
    public:
        virtual std::vector<uint8_t> serialize(const void* obj) = 0;
        virtual void* deserialize(std::vector<uint8_t> &&stream) = 0;

    protected:
        ~ISerializer() = default;
    };

    class ISerializationAdmin {
    public:
        template <typename T>
        std::vector<uint8_t> serialize(const T &obj) {
            return serialize(typeNameHash<T>(), static_cast<const void*>(&obj));
        }

        template <typename T>
        std::unique_ptr<T> deserialize(std::vector<uint8_t> &&stream) {
            auto ret = deserialize(typeNameHash<T>(), std::move(stream));
            return std::unique_ptr<T>(static_cast<T*>(ret));
        }

        template <typename T>
        std::unique_ptr<T> deserialize(const std::vector<uint8_t> &stream) {
            auto ret = deserialize(typeNameHash<T>(), std::vector<uint8_t>{stream});
            return std::unique_ptr<T>(static_cast<T*>(ret));
        }

    protected:
        ~ISerializationAdmin() = default;

        virtual std::vector<uint8_t> serialize(uint64_t type, const void* obj) = 0;
        virtual void* deserialize(uint64_t type, std::vector<uint8_t> &&bytes) = 0;
    };
}