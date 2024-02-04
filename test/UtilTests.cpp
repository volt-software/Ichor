#include "Common.h"
#include <ichor/Common.h>
#include <ctre/ctre.hpp>
#include <regex>

namespace Ichor {
    struct SomeStruct {

    };

    class SomeClass {

    };
}

TEST_CASE("Util Tests") {

    SECTION("CTRE tests") {
        std::string_view input = "/some/http/10/11/12/test";
        auto result = ctre::match<"\\/some\\/http\\/(\\d{1,2})\\/(\\d{1,2})\\/(\\d{1,2})\\/test">(input);
        REQUIRE(result);
        Ichor::constexpr_for<(size_t)0, result.count(), (size_t)1>([&result](auto i) {
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

    SECTION("Typename tests") {
        fmt::print("{}\n", Ichor::typeName<int>());
        fmt::print("{}\n", Ichor::typeName<Ichor::SomeStruct>());
        fmt::print("{}\n", Ichor::typeName<Ichor::SomeClass>());
        REQUIRE(Ichor::typeName<int>() == "int");
        REQUIRE(Ichor::typeName<Ichor::SomeStruct>() == "Ichor::SomeStruct");
        REQUIRE(Ichor::typeName<Ichor::SomeClass>() == "Ichor::SomeClass");
    }

    SECTION("TypenameHash tests") {
        constexpr std::string_view intName = "int";
        constexpr std::string_view someStructName = "Ichor::SomeStruct";
        constexpr std::string_view someClassName = "Ichor::SomeClass";
        REQUIRE(Ichor::typeNameHash<int>() == consteval_wyhash(intName.data(), intName.size(), 0));
        REQUIRE(Ichor::typeNameHash<Ichor::SomeStruct>() == consteval_wyhash(someStructName.data(), someStructName.size(), 0));
        REQUIRE(Ichor::typeNameHash<Ichor::SomeClass>() == consteval_wyhash(someClassName.data(), someClassName.size(), 0));
        REQUIRE(Ichor::typeNameHash<int>() == 8938001142359040884UL);
        REQUIRE(Ichor::typeNameHash<Ichor::SomeStruct>() == 6023179687158125491UL);
        REQUIRE(Ichor::typeNameHash<Ichor::SomeClass>() == 10344807212141480755UL);
    }
}
