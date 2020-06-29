#pragma once

#include <unordered_map>
#include <framework/Service.h>
#include <framework/interfaces/ISerializationAdmin.h>
#include <optional_bundles/logging_bundle/Logger.h>

namespace Cppelix {

    class SerializationAdmin : public ISerializationAdmin, public Service {
    public:
        SerializationAdmin();
        ~SerializationAdmin() final;

        std::vector<uint8_t> serialize(const uint64_t type, const void* obj) final;
        void* deserialize(const uint64_t type, std::vector<uint8_t> &&bytes) final;
        void addSerializer(const uint64_t type, ISerializer* serializer) final;
        void removeSerializer(const uint64_t type) final;

        void addDependencyInstance(ILogger *logger);
        void removeDependencyInstance(ILogger *logger);

        bool start() final;
        bool stop() final;
    private:
        std::unordered_map<uint64_t, ISerializer*> _serializers{};
        ILogger *_logger{nullptr};
        DependencyManager *_mng{nullptr};
    };
}