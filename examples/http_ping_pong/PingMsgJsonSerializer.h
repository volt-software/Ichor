#pragma once

#include <ichor/services/serialization/ISerializer.h>
#include "PingMsg.h"

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
        glz::write_json(msg, buf);
        buf.push_back('\0');
        return buf;
    }
    std::optional<PingMsg> deserialize(std::vector<uint8_t> &&stream) final {
        PingMsg msg;
        auto err = glz::read_json(msg, stream);

        if(err) {
            fmt::print("Glaze error {} at {}\n", (int)err.ec, err.location);
            fmt::print("json {}\n", (char*)stream.data());
            return std::nullopt;
        }

        return msg;
    }
};
