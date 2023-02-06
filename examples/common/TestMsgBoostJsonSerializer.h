#pragma once

#ifdef ICHOR_USE_BOOST_JSON

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/dependency_management/ILifecycleManager.h>
#include "TestMsg.h"

#pragma GCC diagnostic push
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wduplicated-branches"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#pragma GCC diagnostic ignored "-Warray-bounds"
#include <boost/json.hpp>
#pragma GCC diagnostic pop

using namespace Ichor;

class TestMsgBoostJsonSerializer final : public ISerializer<TestMsg>, public AdvancedService<TestMsgBoostJsonSerializer> {
public:
    TestMsgBoostJsonSerializer() = default;
    ~TestMsgBoostJsonSerializer() final = default;

    std::vector<uint8_t> serialize(TestMsg const &msg) final {
        boost::json::value jv = {{"id", msg.id}, {"val", msg.val}};
        auto str = boost::json::serialize(jv);
        return std::vector<uint8_t>(str.begin(), str.end());
    }

    std::optional<TestMsg> deserialize(std::vector<uint8_t> &&stream) final {
        unsigned char buffer[4096];
        boost::json::monotonic_resource mr{buffer};
        boost::json::value value = boost::json::parse(std::string_view{reinterpret_cast<char*>(stream.data()), stream.size()}, &mr);
        return TestMsg{boost::json::value_to<uint64_t>(value.at("id")), boost::json::value_to<std::string>(value.at("val"))};
    }
};

#endif
