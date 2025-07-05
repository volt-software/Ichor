#pragma once

#include <ichor/services/serialization/ISerializer.h>
#include "TestMsg.h"
#include <ichor/glaze.h>

using namespace Ichor;
using namespace Ichor::v1;

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
        auto err = glz::write_json(msg, buf);
        if(err) {
            fmt::println("couldn't serialize {}", glz::nameof(err.ec));
            return {};
        }
        buf.push_back('\0');
        return buf;
    }

    tl::optional<TestMsg> deserialize(std::span<uint8_t const> stream) final {
        TestMsg msg;
        auto err = glz::read_json(msg, stream);

        if(err) {
            fmt::print("Glaze error {} at {}\n", (int)err.ec, err.location);
            fmt::print("json {}\n", std::string_view{(char*)stream.data(), stream.size()});
            return tl::nullopt;
        }

        return msg;
    }
};
