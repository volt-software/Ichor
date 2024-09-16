#pragma once

#include <ichor/services/serialization/ISerializer.h>
#include "PingMsg.h"
#include <ichor/glaze.h>

using namespace Ichor;

template <>
struct glz::meta<PingMsg> {
    using T = PingMsg;
    static constexpr auto value = object(
            "sequence", &T::sequence
    );
};

class PingMsgJsonSerializer final : public ISerializer<PingMsg> {
public:
    std::vector<uint8_t> serialize(PingMsg const &msg) final {
        std::vector<uint8_t> buf;
        auto err = glz::write_json(msg, buf);
        if(err) {
            fmt::println("couldn't serialize {}", glz::nameof(err.ec));
            return {};
        }
        buf.push_back('\0');
        return buf;
    }
    tl::optional<PingMsg> deserialize(std::span<uint8_t const> stream) final {
        PingMsg msg;
        auto err = glz::read_json(msg, stream);

        if(err) {
            fmt::print("Glaze error {} at {}\n", (int)err.ec, err.location);
            fmt::print("json {}\n", (char*)stream.data());
            return tl::nullopt;
        }

        return msg;
    }
};
