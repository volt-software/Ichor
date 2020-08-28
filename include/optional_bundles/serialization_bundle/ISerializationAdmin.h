#pragma once

#include <vector>
#include <memory>

namespace Cppelix {

    class ISerializer : public virtual IService {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        ~ISerializer() override = default;

        virtual std::vector<uint8_t> serialize(const void* obj) = 0;
        virtual void* deserialize(std::vector<uint8_t> &&stream) = 0;
    };

    class ISerializationAdmin : public virtual IService {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        virtual void addSerializer(const uint64_t type, ISerializer* serializer) = 0;
        virtual void removeSerializer(const uint64_t type) = 0;

        template <typename T>
        std::vector<uint8_t> serialize(const T &obj) {
            return serialize(typeNameHash<T>(), static_cast<const void*>(&obj));
        }

        template <typename T>
        std::unique_ptr<T> deserialize(std::vector<uint8_t> &&stream) {
            return std::unique_ptr<T>(static_cast<T*>(deserialize(typeNameHash<T>(), std::move(stream))));
        }

        template <typename T>
        std::unique_ptr<T> deserialize(const std::vector<uint8_t> &stream) {
            return std::unique_ptr<T>(static_cast<T*>(deserialize(typeNameHash<T>(), std::vector<uint8_t>{stream})));
        }

    protected:
        ~ISerializationAdmin() override = default;

        virtual std::vector<uint8_t> serialize(const uint64_t type, const void* obj) = 0;
        virtual void* deserialize(const uint64_t type, std::vector<uint8_t> &&bytes) = 0;
    };
}