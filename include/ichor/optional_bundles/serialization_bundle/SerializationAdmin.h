#pragma once

#include <unordered_map>
#include <ichor/Service.h>
#include <ichor/optional_bundles/serialization_bundle/ISerializationAdmin.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>

namespace Ichor {

    class SerializationAdmin final : public ISerializationAdmin, public Service<SerializationAdmin> {
    public:
        SerializationAdmin(DependencyRegister &reg, IchorProperties props, DependencyManager *mng);
        ~SerializationAdmin() final = default;

        std::vector<uint8_t> serialize(uint64_t type, const void* obj) final;
        void* deserialize(uint64_t type, std::vector<uint8_t> &&bytes) final;

        void addDependencyInstance(ILogger *logger);
        void removeDependencyInstance(ILogger *logger);
        void addDependencyInstance(ISerializer *serializer);
        void removeDependencyInstance(ISerializer *serializer);
    private:

        std::pmr::unordered_map<uint64_t, ISerializer*> _serializers;
        ILogger *_logger{nullptr};
    };
}