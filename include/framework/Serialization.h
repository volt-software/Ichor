#pragma once

#include <unordered_map>
#include <framework/Bundle.h>
#include <framework/interfaces/ISerialization.h>
#include <framework/interfaces/IFrameworkLogger.h>

namespace Cppelix {

    class Serialization : public ISerialization, public Bundle {
    public:
        Serialization();
        ~Serialization() final = default;

        std::vector<uint8_t> serialize(const std::string type, const void* obj) final;
        void* deserialize(const std::string type, const std::vector<uint8_t> &bytes) final;
        void addSerializer(const std::string type, std::unique_ptr<ISerializer> serializer) final;
        void removeSerializer(const std::string type) final;

        void addDependencyInstance(IFrameworkLogger *logger);

        bool start() final;
        bool stop() final;
    private:
        std::unordered_map<std::string, std::unique_ptr<ISerializer>> _serializers;
        IFrameworkLogger *_logger;
    };
}