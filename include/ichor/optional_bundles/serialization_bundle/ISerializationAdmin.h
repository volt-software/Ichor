#pragma once

#include <vector>
#include <memory>
#include <tuple>

namespace Ichor {

    class ISerializer {
    public:
        virtual std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>> serialize(const void* obj) = 0;
        virtual void* deserialize(std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>> &&stream) = 0;

    protected:
        ~ISerializer() = default;
    };

    class ISerializationAdmin {
    public:
        template <typename T>
        std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>> serialize(const T &obj) {
            return serialize(typeNameHash<T>(), static_cast<const void*>(&obj));
        }

        template <typename T>
        Ichor::unique_ptr<T> deserialize(std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>> &&stream) {
            auto ret = deserialize(typeNameHash<T>(), std::move(stream));
            return Ichor::unique_ptr<T>(static_cast<T*>(std::get<0>(ret)), Deleter{InternalDeleter<T>{std::get<1>(ret)}});
        }

        template <typename T>
        Ichor::unique_ptr<T> deserialize(const std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>> &stream) {
            auto ret = deserialize(typeNameHash<T>(), std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>>{stream});
            return Ichor::unique_ptr<T>(static_cast<T*>(std::get<0>(ret)), Deleter{InternalDeleter<T>{std::get<1>(ret)}});
        }

    protected:
        ~ISerializationAdmin() = default;

        virtual std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>> serialize(uint64_t type, const void* obj) = 0;
        virtual std::tuple<void*, std::pmr::memory_resource*> deserialize(uint64_t type, std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>> &&bytes) = 0;
    };
}