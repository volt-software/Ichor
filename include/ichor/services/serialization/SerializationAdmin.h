#pragma once

#include <unordered_map>
#include <ichor/Service.h>
#include <ichor/services/serialization/ISerializationAdmin.h>
#include <ichor/services/logging/Logger.h>

namespace Ichor {

    class SerializationAdmin final : public ISerializationAdmin, public Service<SerializationAdmin> {
    public:
        SerializationAdmin(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~SerializationAdmin() final = default;

        std::vector<uint8_t> serialize(uint64_t type, const void* obj) final;
        void* deserialize(uint64_t type, std::vector<uint8_t> &&bytes) final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);
        void addDependencyInstance(ISerializer *serializer, IService *isvc);
        void removeDependencyInstance(ISerializer *serializer, IService *isvc);
    private:

        unordered_map<uint64_t, ISerializer*> _serializers;
        ILogger *_logger{nullptr};
    };
}