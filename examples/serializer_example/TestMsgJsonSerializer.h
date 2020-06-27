#pragma once

#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "../include/framework/Service.h"
#include "../include/framework/Framework.h"
#include "../include/framework/SerializationAdmin.h"
#include "framework/ServiceLifecycleManager.h"
#include "TestMsg.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

using namespace Cppelix;

class TestMsgJsonSerializer : public ISerializer, public Service {
public:
    ~TestMsgJsonSerializer() final = default;
    bool start() final {
        LOG_INFO(_logger, "TestMsgSerializer started");
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "TestMsgSerializer stopped");
        return true;
    }

    void addDependencyInstance(IFrameworkLogger *logger) {
        _logger = logger;
        LOG_INFO(_logger, "Inserted logger");
    }

    void removeDependencyInstance(IFrameworkLogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = serializationAdmin;
        LOG_INFO(_logger, "Inserted serializationAdmin");
        _serializationAdmin->addSerializer(typeNameHash<TestMsg>(), this);
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
        return std::vector<uint8_t>(ret, ret + sb.GetSize());
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
    IFrameworkLogger *_logger;
    ISerializationAdmin *_serializationAdmin;
};