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

    class ISerialization {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        virtual std::vector<uint8_t> serialize(const std::string type, const void* obj) = 0;
        virtual void* deserialize(const std::string type, const std::vector<uint8_t> &bytes) = 0;
        virtual void addSerializer(const std::string type, std::unique_ptr<ISerializer> serializer) = 0;
        virtual void removeSerializer(const std::string type) = 0;

    protected:
        virtual ~ISerialization() = default;
    };
}