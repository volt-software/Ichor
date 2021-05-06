#pragma once

#include <vector>
#include <memory>
#include <tuple>

namespace Ichor {

    class ISerializer {
    public:
        virtual std::pmr::vector<uint8_t> serialize(const void* obj) = 0;
        virtual void* deserialize(std::pmr::vector<uint8_t> &&stream) = 0;

    protected:
        virtual ~ISerializer() = default;
    };

    class ISerializationAdmin {
    public:
        template <typename T>
        std::pmr::vector<uint8_t> serialize(const T &obj) {
            return serialize(typeNameHash<T>(), static_cast<const void*>(&obj));
        }

        template <typename T>
        std::unique_ptr<T, Deleter> deserialize(std::pmr::vector<uint8_t> &&stream) {
            auto ret = deserialize(typeNameHash<T>(), std::move(stream));
            return std::unique_ptr<T, Deleter>(static_cast<T*>(std::get<0>(ret)), Deleter{std::get<1>(ret), sizeof(T)});
        }

        template <typename T>
        std::unique_ptr<T, Deleter> deserialize(const std::pmr::vector<uint8_t> &stream) {
            auto ret = deserialize(typeNameHash<T>(), std::pmr::vector<uint8_t>{stream});
            return std::unique_ptr<T, Deleter>(static_cast<T*>(std::get<0>(ret)), Deleter{std::get<1>(ret), sizeof(T)});
        }

    protected:
        virtual ~ISerializationAdmin() = default;

        virtual std::pmr::vector<uint8_t> serialize(uint64_t type, const void* obj) = 0;
        virtual std::tuple<void*, std::pmr::memory_resource*> deserialize(uint64_t type, std::pmr::vector<uint8_t> &&bytes) = 0;
    };
}