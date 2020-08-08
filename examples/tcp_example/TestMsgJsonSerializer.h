#pragma once

#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <optional_bundles/logging_bundle/Logger.h>
#include "../include/framework/Service.h"
#include "optional_bundles/serialization_bundle/SerializationAdmin.h"
#include "framework/LifecycleManager.h"
#include "TestMsg.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

using namespace Cppelix;

class TestMsgJsonSerializer final : public ISerializer, public Service {
public:
    ~TestMsgJsonSerializer() final = default;
    bool start() final {
        LOG_INFO(_logger, "TestMsgSerializer started");
        _serializationAdmin->addSerializer(typeNameHash<TestMsg>(), this);
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "TestMsgSerializer stopped");
        _serializationAdmin->removeSerializer(typeNameHash<TestMsg>());
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
        LOG_TRACE(_logger, "Inserted logger");
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = serializationAdmin;
        LOG_INFO(_logger, "Inserted serializationAdmin");
    }

    void removeDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = nullptr;
        LOG_INFO(_logger, "Removed serializationAdmin");
    }

    std::vector<uint8_t> serialize(const void* obj) final {
        auto msg = static_cast<const TestMsg*>(obj);
        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

        writer.StartObject();

        writer.String("id");
        writer.Uint64(msg->id);

        writer.String("val");
        writer.String(msg->val.c_str(), msg->val.size());

        writer.EndObject();
        auto *ret = sb.GetString();
        return std::vector<uint8_t>(ret, ret + sb.GetSize() + 1);
    }
    void* deserialize(std::vector<uint8_t> &&stream) final {
        rapidjson::Document d;
        d.ParseInsitu(reinterpret_cast<char*>(stream.data()));

        if(d.HasParseError() || !d.HasMember("id") || !d.HasMember("val")) {
            LOG_ERROR(_logger, "stream not valid json");
            return nullptr;
        }

        return new TestMsg(d["id"].GetUint64(), d["val"].GetString());
    }

private:
    ILogger *_logger;
    ISerializationAdmin *_serializationAdmin;
};