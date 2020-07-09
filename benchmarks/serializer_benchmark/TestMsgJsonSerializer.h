#pragma once

#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <optional_bundles/logging_bundle/Logger.h>
#include "../include/framework/Service.h"
#include "../include/framework/SerializationAdmin.h"
#include "framework/ServiceLifecycleManager.h"
#include "TestMsg.h"
#include <iostream>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#ifdef USE_SIMDJSON
#include "simdjson.h"
#endif

using namespace Cppelix;

class TestMsgJsonSerializer : public ISerializer, public Service {
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
        LOG_INFO(_logger, "Inserted logger");
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

    std::vector<uint8_t> serialize(const void* obj) {
        auto msg = static_cast<const TestMsg*>(obj);
        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

        writer.StartObject();

        writer.String("id", 2);
        writer.Uint64(msg->id);

        writer.String("val", 3);
        writer.String(msg->val.c_str(), msg->val.size());

        writer.EndObject();
        auto *ret = sb.GetString();
        return std::vector<uint8_t>(ret, ret + sb.GetSize());
    }

    void* deserialize(std::vector<uint8_t> &&stream) {
#ifdef USE_SIMDJSON
        auto d = parser.parse(stream.data(), stream.size());

        return new TestMsg(d["id"], std::string{d["val"].get_string().value()});
#else
        rapidjson::Document d;
        d.ParseInsitu(reinterpret_cast<char *>(stream.data()));

        if (d.HasParseError() || !d.HasMember("id") || !d.HasMember("val")) {
            LOG_ERROR(_logger, "stream not valid json");
            return nullptr;
        }

        return new TestMsg(d["id"].GetUint64(), d["val"].GetString());
#endif
    }

private:
    ILogger *_logger;
    ISerializationAdmin *_serializationAdmin;
#ifdef USE_SIMDJSON
    simdjson::dom::parser parser;
#endif
};