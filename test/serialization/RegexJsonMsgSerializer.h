#pragma once

#include <ichor/services/serialization/ISerializer.h>
#include "RegexJsonMsg.h"
#include <ichor/glaze.h>

using namespace Ichor;

template <>
struct glz::meta<RegexJsonMsg> {
    using T = RegexJsonMsg;
    static constexpr auto value = object(
            "query_params", &T::query_params
    );
};

class RegexJsonMsgSerializer final : public ISerializer<RegexJsonMsg> {
public:
    std::vector<uint8_t> serialize(RegexJsonMsg const &msg) final {
        std::vector<uint8_t> buf;
        glz::write_json(msg, buf);
        buf.push_back('\0');
        return buf;
    }

    tl::optional<RegexJsonMsg> deserialize(std::vector<uint8_t> &&stream) final {
        RegexJsonMsg msg;
        auto err = glz::read_json(msg, stream);

        if(err) {
            fmt::print("Glaze error {} at {}\n", (int)err.ec, err.location);
            fmt::print("json {}\n", (char*)stream.data());
            return tl::nullopt;
        }

        return msg;
    }
};
