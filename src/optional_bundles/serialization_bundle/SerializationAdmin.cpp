#include <ichor/optional_bundles/serialization_bundle/SerializationAdmin.h>
#include <ichor/DependencyManager.h>

Ichor::SerializationAdmin::SerializationAdmin(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng), _serializers(getMemoryResource()) {
    reg.registerDependency<ILogger>(this, true);
    reg.registerDependency<ISerializer>(this, false);
}

std::vector<uint8_t> Ichor::SerializationAdmin::serialize(const uint64_t type, const void* obj) {
    auto serializer = _serializers.find(type);
    if(serializer == end(_serializers)) {
        throw std::runtime_error(fmt::format("Couldn't find serializer for type {}", type));
    }
    return serializer->second->serialize(obj);
}

void* Ichor::SerializationAdmin::deserialize(const uint64_t type, std::vector<uint8_t> &&bytes) {
    auto serializer = _serializers.find(type);
    if(serializer == end(_serializers)) {
        throw std::runtime_error(fmt::format("Couldn't find serializer for type {}", type));
    }
    return serializer->second->deserialize(std::move(bytes));
}

void Ichor::SerializationAdmin::addDependencyInstance(ILogger *logger) {
    _logger = logger;
    ICHOR_LOG_TRACE(_logger, "Inserted logger");
}

void Ichor::SerializationAdmin::removeDependencyInstance(ILogger *logger) {
    _logger = nullptr;
}

void Ichor::SerializationAdmin::addDependencyInstance(ISerializer *serializer) {
    if(!serializer->getProperties()->contains("type")) {
        ICHOR_LOG_TRACE(_logger, "No type property for serializer {}", serializer->getServiceId());
        return;
    }

    auto type = Ichor::any_cast<uint64_t>(serializer->getProperties()->operator[]("type"));

    auto existingSerializer = _serializers.find(type);
    if(existingSerializer != end(_serializers)) {
        ICHOR_LOG_TRACE(_logger, "Serializer for type {} already added", type);
        return;
    }

    _serializers.emplace(type, serializer);
    ICHOR_LOG_TRACE(_logger, "Inserted serializer for type {}", type);
}

void Ichor::SerializationAdmin::removeDependencyInstance(ISerializer *serializer) {
    if(!serializer->getProperties()->contains("type")) {
        ICHOR_LOG_TRACE(_logger, "No type property for serializer {}", serializer->getServiceId());
    }

    auto type = Ichor::any_cast<uint64_t>(serializer->getProperties()->operator[]("type"));

    auto existingSerializer = _serializers.find(type);
    if(existingSerializer == end(_serializers)) {
        ICHOR_LOG_TRACE(_logger, "Couldn't find serializer for type {} to remove", type);
        return;
    }

    _serializers.erase(type);
    ICHOR_LOG_TRACE(_logger, "Removed serializer for type {}", type);
}
