#include <spdlog/spdlog.h>
#include <framework/Serialization.h>

Cppelix::Serialization::Serialization() {

}

std::vector<uint8_t> Cppelix::Serialization::serialize(const std::string type, const void* obj) {
    auto serializer = _serializers.find(type);
    if(serializer == end(_serializers)) {
        throw std::runtime_error("Couldn't find serializer for type" + type);
    }
    LOG_INFO(_logger, "Serializing for type {}", type);
    return serializer->second->serialize(obj);
}

void* Cppelix::Serialization::deserialize(const std::string type, const std::vector<uint8_t> &bytes) {
    auto serializer = _serializers.find(type);
    if(serializer == end(_serializers)) {
        throw std::runtime_error("Couldn't find serializer for type" + type);
    }
    LOG_INFO(_logger, "Deserializing for type {}", type);
    return serializer->second->deserialize(bytes);
}

void Cppelix::Serialization::addSerializer(const std::string type, std::unique_ptr<Cppelix::ISerializer> _serializer) {
    auto serializer = _serializers.find(type);
    if(serializer != end(_serializers)) {
        throw std::runtime_error("Serializer already added for type" + type);
    }
    _serializers.emplace(type, std::move(_serializer));
    LOG_INFO(_logger, "Inserted serializer for type {}", type);
}

void Cppelix::Serialization::removeSerializer(const std::string type) {
    auto serializer = _serializers.find(type);
    if(serializer != end(_serializers)) {
        throw std::runtime_error("Serializer already removed for type" + type);
    }
    _serializers.erase(type);
    LOG_INFO(_logger, "Removed serializer for type {}", type);
}

bool Cppelix::Serialization::start() {
    LOG_INFO(_logger, "Start");
    return true;
}

bool Cppelix::Serialization::stop() {
    LOG_INFO(_logger, "Stop");
    return true;
}

void Cppelix::Serialization::addDependencyInstance(IFrameworkLogger *logger) {
    _logger = logger;
    LOG_INFO(_logger, "Inserted logger");
}
