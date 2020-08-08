#include <optional_bundles/serialization_bundle/SerializationAdmin.h>

std::vector<uint8_t> Cppelix::SerializationAdmin::serialize(const uint64_t type, const void* obj) {
    auto serializer = _serializers.find(type);
    if(serializer == end(_serializers)) {
        throw std::runtime_error(fmt::format("Couldn't find serializer for type {}", type));
    }
    return serializer->second->serialize(obj);
}

void* Cppelix::SerializationAdmin::deserialize(const uint64_t type, std::vector<uint8_t> &&bytes) {
    auto serializer = _serializers.find(type);
    if(serializer == end(_serializers)) {
        throw std::runtime_error(fmt::format("Couldn't find serializer for type {}", type));
    }
    return serializer->second->deserialize(std::move(bytes));
}

void Cppelix::SerializationAdmin::addSerializer(const uint64_t type, Cppelix::ISerializer* _serializer) {
    auto serializer = _serializers.find(type);
    if(serializer != end(_serializers)) {
        throw std::runtime_error(fmt::format("Serializer for type {} already added", type));
    }
    _serializers.emplace(type, _serializer);
    LOG_TRACE(_logger, "Inserted serializer for type {}", type);
}

void Cppelix::SerializationAdmin::removeSerializer(const uint64_t type) {
    auto serializer = _serializers.find(type);
    if(serializer == end(_serializers)) {
        throw std::runtime_error(fmt::format("Couldn't find serializer for type {}", type));
    }
    _serializers.erase(type);
    LOG_TRACE(_logger, "Removed serializer for type {}", type);
}

bool Cppelix::SerializationAdmin::start() {
    LOG_TRACE(_logger, "Start");
    return true;
}

bool Cppelix::SerializationAdmin::stop() {
    LOG_TRACE(_logger, "Stop");
    return true;
}

void Cppelix::SerializationAdmin::addDependencyInstance(ILogger *logger) {
    _logger = logger;
    LOG_TRACE(_logger, "Inserted logger");
}

void Cppelix::SerializationAdmin::removeDependencyInstance(ILogger *logger) {
    _logger = nullptr;
}
