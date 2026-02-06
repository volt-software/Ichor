#include "Common.h"
#include <ichor/Common.h>
#include <ctre/ctre.hpp>

namespace Ichor {
    struct SomeStruct {

    };

    class SomeClass {

    };

    template <size_t N>
    struct TypeTag {
    };

    template <size_t... Is>
    constexpr auto makeTypeTagHashes(std::index_sequence<Is...>) {
        return std::array<NameHashType, sizeof...(Is)>{ typeNameHash<TypeTag<Is>>()... };
    }
}

TEST_CASE("UtilTests: CTRE tests") {
    std::string_view input = "/some/http/10/11/12/test";
    auto result = ctre::match<"\\/some\\/http\\/(\\d{1,2})\\/(\\d{1,2})\\/(\\d{1,2})\\/test">(input);
    REQUIRE(result);
    REQUIRE(result.count() == 4);
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

TEST_CASE("UtilTests: Typename tests") {
    fmt::print("{}\n", Ichor::typeName<int>());
    fmt::print("{}\n", Ichor::typeName<Ichor::SomeStruct>());
    fmt::print("{}\n", Ichor::typeName<Ichor::SomeClass>());
    STATIC_REQUIRE(Ichor::typeName<int>() == "int");
    STATIC_REQUIRE(Ichor::typeName<Ichor::SomeStruct>() == "Ichor::SomeStruct");
    STATIC_REQUIRE(Ichor::typeName<Ichor::SomeClass>() == "Ichor::SomeClass");
    REQUIRE(Ichor::typeName<int>() == "int");
    REQUIRE(Ichor::typeName<Ichor::SomeStruct>() == "Ichor::SomeStruct");
    REQUIRE(Ichor::typeName<Ichor::SomeClass>() == "Ichor::SomeClass");
}

TEST_CASE("UtilTests: TypenameHash tests") {
    constexpr std::string_view intName = "int";
    constexpr std::string_view someStructName = "Ichor::SomeStruct";
    constexpr std::string_view someClassName = "Ichor::SomeClass";
    STATIC_REQUIRE(Ichor::typeNameHash<int>() == 15108630419520569278UL);
    STATIC_REQUIRE(Ichor::typeNameHash<Ichor::SomeStruct>() == 18010430330845319337UL);
    STATIC_REQUIRE(Ichor::typeNameHash<Ichor::SomeClass>() == 4213501378944759609UL);
    REQUIRE(Ichor::typeNameHash<int>() == rapidhash(intName.data(), intName.size()));
    REQUIRE(Ichor::typeNameHash<Ichor::SomeStruct>() == rapidhash(someStructName.data(), someStructName.size()));
    REQUIRE(Ichor::typeNameHash<Ichor::SomeClass>() == rapidhash(someClassName.data(), someClassName.size()));
    REQUIRE(Ichor::typeNameHash<int>() == 15108630419520569278UL);
    REQUIRE(Ichor::typeNameHash<Ichor::SomeStruct>() == 18010430330845319337UL);
    REQUIRE(Ichor::typeNameHash<Ichor::SomeClass>() == 4213501378944759609UL);
}

TEST_CASE("UtilTests: TypenameHash collision smoke test") {
    constexpr size_t kCount = 8192;
    constexpr auto hashes = makeTypeTagHashes(std::make_index_sequence<kCount>{});

    std::unordered_map<NameHashType, size_t> seen;
    seen.reserve(hashes.size());
    REQUIRE(hashes.size() == kCount);

    for (size_t i = 0; i < hashes.size(); ++i) {
        auto [it, inserted] = seen.emplace(hashes[i], i);
        if(!inserted) {
            FAIL(fmt::format("typeNameHash collision: TypeTag<{}> vs TypeTag<{}>, hash={}", it->second, i, hashes[i]));
        }
    }
}
