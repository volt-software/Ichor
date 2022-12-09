#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/Service.h>
#include <ichor/services/serialization/ISerializer.h>
#include "ichor/dependency_management/ILifecycleManager.h"
#include "TestMsg.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

using namespace Ichor;

class TestMsgRapidJsonSerializer final : public ISerializer<TestMsg>, public Service<TestMsgRapidJsonSerializer> {
public:
    TestMsgRapidJsonSerializer() = default;
    ~TestMsgRapidJsonSerializer() final = default;

    std::vector<uint8_t> serialize(TestMsg const &msg) final {
        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

        writer.StartObject();

        writer.String("id");
        writer.Uint64(msg.id);

        writer.String("val");
        writer.String(msg.val.c_str(), static_cast<uint32_t>(msg.val.size()));

        writer.EndObject();
        auto *ret = sb.GetString();
        return std::vector<uint8_t>(ret, ret + sb.GetSize() + 1);
    }

    std::optional<TestMsg> deserialize(std::vector<uint8_t> &&stream) final {
        rapidjson::Document d;
        d.ParseInsitu(reinterpret_cast<char*>(stream.data()));

        if(d.HasParseError() || !d.HasMember("id") || !d.HasMember("val")) {
            return {};
        }

        return TestMsg{d["id"].GetUint64(), d["val"].GetString()};
    }
};
