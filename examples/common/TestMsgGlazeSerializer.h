#pragma once

#include <ichor/services/serialization/ISerializer.h>
#include "TestMsg.h"

// Glaze uses different conventions than Ichor, ignore them to prevent being spammed by warnings
#if defined( __GNUC__ )
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#    pragma GCC diagnostic ignored "-Wshadow"
#    pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <glaze/glaze.hpp>
#if defined( __GNUC__ )
#    pragma GCC diagnostic pop
#endif

using namespace Ichor;

template <>
struct glz::meta<TestMsg> {
    using T = TestMsg;
    static constexpr auto value = object(
            "id", &T::id,
            "val", &T::val
    );
};

class TestMsgGlazeSerializer final : public ISerializer<TestMsg> {
public:
    std::vector<uint8_t> serialize(TestMsg const &msg) final {
        std::vector<uint8_t> buf;
        glz::write_json(msg, buf);
        buf.push_back('\0');
        return buf;
    }

    std::optional<TestMsg> deserialize(std::vector<uint8_t> &&stream) final {
        TestMsg msg;
        auto err = glz::read_json(msg, stream);

        if(err) {
            fmt::print("Glaze error {} at {}\n", (int)err.ec, err.location);
            fmt::print("json {}\n", (char*)stream.data());
            return std::nullopt;
        }

        return msg;
    }
};
