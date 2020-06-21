#pragma once

#include <vector>
#include <memory>

namespace Cppelix {

    class ISerializer {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        virtual ~ISerializer() = default;

        virtual std::vector<uint8_t> serialize(const void* obj) = 0;
        virtual void* deserialize(const std::vector<uint8_t> &stream) = 0;
    };

    class ISerializationAdmin {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        virtual void addSerializer(const std::string_view type, ISerializer* serializer) = 0;
        virtual void removeSerializer(const std::string_view type) = 0;

        template <typename T>
        std::vector<uint8_t> serialize(const T &obj) {
            return serialize(typeName<T>(), static_cast<const void*>(&obj));
        }

        template <typename T>
        std::unique_ptr<T> deserialize(const std::vector<uint8_t> &stream) {
            return std::unique_ptr<T>(static_cast<T*>(deserialize(typeName<T>(), stream)));
        }

    protected:
        virtual ~ISerializationAdmin() = default;

        virtual std::vector<uint8_t> serialize(const std::string_view type, const void* obj) = 0;
        virtual void* deserialize(const std::string_view type, const std::vector<uint8_t> &bytes) = 0;
    };
}