#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/optional_bundles/serialization_bundle/ISerializationAdmin.h>
#include <ichor/LifecycleManager.h>
#include "TestMsg.h"

#ifdef USE_RAPIDJSON
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#elif USE_BOOST_JSON
#pragma GCC diagnostic push
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wduplicated-branches"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#pragma GCC diagnostic ignored "-Warray-bounds"
#include <boost/json/src.hpp>
#pragma GCC diagnostic pop
#endif

using namespace Ichor;

class TestMsgJsonSerializer final : public ISerializer, public Service<TestMsgJsonSerializer> {
public:
    TestMsgJsonSerializer(Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        _properties.insert({"type", Ichor::make_any<uint64_t>(typeNameHash<TestMsg>())});
    }
    ~TestMsgJsonSerializer() final = default;

    std::vector<uint8_t> serialize(const void* obj) final {
        auto msg = static_cast<const TestMsg*>(obj);

#ifdef USE_RAPIDJSON
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
#elif USE_BOOST_JSON
        boost::json::value jv{};
        boost::json::serializer sr;
        jv = {{"id", msg->id}, {"val", msg->val}};
        sr.reset(&jv);

        std::string_view sv;
        char buf[4096];
        sv = sr.read(buf);

        if(sr.done()) {
            return std::vector<uint8_t>(sv.data(), sv.data() + sv.size());
        }

        std::vector<uint8_t> ret{};
        std::size_t len = sv.size();
        ret.reserve(len*2);
        ret.resize(ret.capacity());
        std::memcpy(ret.data(), sv.data(), sv.size());

        for(;;) {
            sv = sr.read(reinterpret_cast<char*>(ret.data()) + len, ret.size() - len);
            len += sv.size();
            if(sr.done()) {
                break;
            }
            ret.resize(ret.capacity()*1.5);
        }
        ret.resize(len);

        return ret;
#endif
    }
    void* deserialize(std::vector<uint8_t> &&stream) final {
#ifdef USE_RAPIDJSON
        rapidjson::Document d;
        d.ParseInsitu(reinterpret_cast<char*>(stream.data()));

        if(d.HasParseError() || !d.HasMember("id") || !d.HasMember("val")) {
            return nullptr;
        }

        return new TestMsg{d["id"].GetUint64(), d["val"].GetString()};
#elif USE_BOOST_JSON
        boost::json::parser p{};

        std::error_code ec;
        auto size = p.write(reinterpret_cast<char*>(stream.data()), stream.size(), ec);

        if(ec || size != stream.size()) {
            return nullptr;
        }

        auto value = p.release();

        return new TestMsg{boost::json::value_to<uint64_t>(value.at("id")), boost::json::value_to<std::string>(value.at("val"))};
#endif
    }
};