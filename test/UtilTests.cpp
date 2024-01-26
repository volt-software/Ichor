#include "Common.h"
#include <ichor/Common.h>
#include <ctre/ctre.hpp>
#include <regex>

using namespace Ichor;


TEST_CASE("Util Tests") {

    SECTION("FastAtoiCompare(u) tests") {
        REQUIRE(FastAtoiCompareu("10") == 10);
        REQUIRE(FastAtoiCompareu("0") == 0);
        REQUIRE(FastAtoiCompareu("u10") == 0);
        REQUIRE(FastAtoiCompareu("10u") == 10);
        REQUIRE(FastAtoiCompareu(std::to_string(std::numeric_limits<uint64_t>::max()).c_str()) == std::numeric_limits<uint64_t>::max());
        REQUIRE(FastAtoiCompare("10") == 10);
        REQUIRE(FastAtoiCompare("0") == 0);
        REQUIRE(FastAtoiCompare("u10") == 0);
        REQUIRE(FastAtoiCompare("10u") == 10);
        REQUIRE(FastAtoiCompare("-10") == -10);
        REQUIRE(FastAtoiCompare(std::to_string(std::numeric_limits<int64_t>::max()).c_str()) == std::numeric_limits<int64_t>::max());
        REQUIRE(FastAtoiCompare(std::to_string(std::numeric_limits<int64_t>::min()).c_str()) == std::numeric_limits<int64_t>::min());
    }

    SECTION("CTRE tests") {
        std::string_view input = "/some/http/10/11/12/test";
        auto result = ctre::match<"\\/some\\/http\\/(\\d{1,2})\\/(\\d{1,2})\\/(\\d{1,2})\\/test">(input);
        REQUIRE(result);
        constexpr_for<(size_t)0, result.count(), (size_t)1>([&result](auto i) {
            fmt::print("ctre match {}\n", result.get<i>().to_view());
            if constexpr (i == 1) {
                REQUIRE(result.get<i>() == "10");
            }
            if constexpr (i == 2) {
                REQUIRE(result.get<i>() == "11");
            }
            if constexpr (i == 3) {
                REQUIRE(result.get<i>() == "12");
            }
        });
    }

    SECTION("std regex tests") {
        std::string_view input = "/some/http/10/11/12/test";
        std::regex _r{"\\/some\\/http\\/(\\d{1,2})\\/(\\d{1,2})\\/(\\d{1,2})\\/test", std::regex::ECMAScript | std::regex::optimize};
        std::match_results<typename decltype(input)::const_iterator> matches;
        auto result = std::regex_match(input.cbegin(), input.cend(), matches, _r);
        REQUIRE(result);
        for(auto &match : matches) {
            fmt::print("std match {}\n", match.str());
        }
    }
}
