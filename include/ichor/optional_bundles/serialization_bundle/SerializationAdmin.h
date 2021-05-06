#pragma once

#include <unordered_map>
#include <ichor/Service.h>
#include <ichor/optional_bundles/serialization_bundle/ISerializationAdmin.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>

namespace Ichor {

    class SerializationAdmin final : public ISerializationAdmin, public Service<SerializationAdmin> {
    public:
        SerializationAdmin(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~SerializationAdmin() final = default;

        std::pmr::vector<uint8_t> serialize(uint64_t type, const void* obj) final;
        std::tuple<void*, std::pmr::memory_resource*> deserialize(uint64_t type, std::pmr::vector<uint8_t> &&bytes) final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);
        void addDependencyInstance(ISerializer *serializer, IService *isvc);
        void removeDependencyInstance(ISerializer *serializer, IService *isvc);
    private:

        std::pmr::unordered_map<uint64_t, ISerializer*> _serializers;
        ILogger *_logger{nullptr};
    };
}