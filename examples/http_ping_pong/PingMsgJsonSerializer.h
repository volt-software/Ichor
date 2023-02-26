#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/serialization/ISerializer.h>
#include "PingMsg.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

using namespace Ichor;

class PingMsgJsonSerializer final : public ISerializer<PingMsg>, public AdvancedService<PingMsgJsonSerializer> {
public:
    PingMsgJsonSerializer() = default;
    ~PingMsgJsonSerializer() final = default;

    std::vector<uint8_t> serialize(PingMsg const &msg) final {
        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

        writer.StartObject();

        writer.String("sequence");
        writer.Uint64(msg.sequence);

        writer.EndObject();
        auto *ret = sb.GetString();
        return std::vector<uint8_t>(ret, ret + sb.GetSize() + 1);
    }
    std::optional<PingMsg> deserialize(std::vector<uint8_t> &&stream) final {
        rapidjson::Document d;
        d.ParseInsitu(reinterpret_cast<char*>(stream.data()));

        if(d.HasParseError() || !d.HasMember("sequence")) {
            return {};
        }

        return PingMsg{d["sequence"].GetUint64()};
    }
};
