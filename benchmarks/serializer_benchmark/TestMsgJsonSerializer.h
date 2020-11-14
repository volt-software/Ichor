#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/optional_bundles/serialization_bundle/ISerializationAdmin.h>
#include <ichor/LifecycleManager.h>
#include "TestMsg.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

using namespace Ichor;

class TestMsgJsonSerializer final : public ISerializer, public Service {
public:
    TestMsgJsonSerializer(DependencyRegister &reg, IchorProperties props) : Service(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
        _properties.insert({"type", typeNameHash<TestMsg>()});
    }
    ~TestMsgJsonSerializer() final = default;
    bool start() final {
        return true;
    }

    bool stop() final {
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
        LOG_TRACE(_logger, "Inserted logger");
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    std::vector<uint8_t> serialize(const void* obj) final {
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
        return std::vector<uint8_t>(ret, ret + sb.GetSize() + 1);
    }

    void* deserialize(std::vector<uint8_t> &&stream) final {
        rapidjson::Document d;
        d.ParseInsitu(reinterpret_cast<char *>(stream.data()));

        if (d.HasParseError() || !d.HasMember("id") || !d.HasMember("val")) {
            LOG_ERROR(_logger, "stream not valid json");
            return nullptr;
        }

        return new TestMsg{d["id"].GetUint64(), d["val"].GetString()};
    }

private:
    ILogger *_logger{nullptr};
};