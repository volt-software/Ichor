#pragma once

#include <unordered_map>
#include <framework/Bundle.h>
#include <framework/interfaces/ISerializationAdmin.h>
#include <framework/interfaces/IFrameworkLogger.h>

namespace Cppelix {

    class SerializationAdmin : public ISerializationAdmin, public Bundle {
    public:
        SerializationAdmin();
        ~SerializationAdmin() final;

        std::vector<uint8_t> serialize(const std::string_view type, const void* obj) final;
        void* deserialize(const std::string_view type, std::vector<uint8_t> &&bytes) final;
        void addSerializer(const std::string_view type, ISerializer* serializer) final;
        void removeSerializer(const std::string_view type) final;

        void addDependencyInstance(IFrameworkLogger *logger);
        void removeDependencyInstance(IFrameworkLogger *logger);

        bool start() final;
        bool stop() final;
    private:
        std::unordered_map<std::string_view, ISerializer*> _serializers{};
        IFrameworkLogger *_logger{nullptr};
        DependencyManager *_mng{nullptr};
    };
}