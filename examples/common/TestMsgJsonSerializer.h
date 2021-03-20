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

class TestMsgJsonSerializer final : public ISerializer, public Service<TestMsgJsonSerializer> {
public:
    TestMsgJsonSerializer(IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
        _properties.insert({"type", Ichor::make_any<uint64_t>(getMemoryResource(), typeNameHash<TestMsg>())});
    }
    ~TestMsgJsonSerializer() final = default;

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
            return nullptr;
        }

        return new TestMsg{d["id"].GetUint64(), d["val"].GetString()};
    }
};