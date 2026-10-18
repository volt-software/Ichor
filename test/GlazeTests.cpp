#include "Common.h"
#include <ichor/glaze.h>

namespace {
    struct KeyLengthsMsg {
        uint64_t abc;
        uint64_t abcde;
        uint64_t abcdef;
        uint64_t abcdefg;
    };

    template <uint64_t Count>
    void requireCompareMatchesEveryPosition() {
        const char lhs[] = "abcdefgh";
        REQUIRE(glz::compare<Count>(lhs, lhs));

        for(uint64_t i = 0; i < Count; i++) {
            char rhs[] = "abcdefgh";
            rhs[i] = 'x';
            REQUIRE(!glz::compare<Count>(lhs, rhs));
        }

        // bytes past Count are not part of the comparison
        char rhs[] = "abcdefgh";
        rhs[Count] = 'x';
        REQUIRE(glz::compare<Count>(lhs, rhs));
    }
}

template <>
struct glz::meta<KeyLengthsMsg> {
    using T = KeyLengthsMsg;
    static constexpr auto value = object(
            "abc", &T::abc,
            "abcde", &T::abcde,
            "abcdef", &T::abcdef,
            "abcdefg", &T::abcdefg
    );
};

// include/ichor/glaze.h overrides glz::compare for these counts
TEST_CASE("GlazeTests: compare overrides") {
    requireCompareMatchesEveryPosition<3>();
    requireCompareMatchesEveryPosition<5>();
    requireCompareMatchesEveryPosition<6>();
    requireCompareMatchesEveryPosition<7>();
}

TEST_CASE("GlazeTests: read keys of overridden lengths") {
    KeyLengthsMsg msg{1, 2, 3, 4};
    std::string buf;
    REQUIRE(!glz::write_json(msg, buf));

    KeyLengthsMsg msg2{};
    REQUIRE(!glz::read_json(msg2, buf));
    REQUIRE(msg2.abc == 1);
    REQUIRE(msg2.abcde == 2);
    REQUIRE(msg2.abcdef == 3);
    REQUIRE(msg2.abcdefg == 4);

    for(std::string_view key : {"abx", "abcdx", "abcdex", "abcdefx", "xbc", "xbcde", "xbcdef", "xbcdefg"}) {
        KeyLengthsMsg msg3{};
        auto json = fmt::format(R"({{"{}":5}})", key);
        auto err = glz::read_json(msg3, json);
        REQUIRE(err.ec == glz::error_code::unknown_key);
    }
}
