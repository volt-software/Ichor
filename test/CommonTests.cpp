#include <catch2/catch_test_macros.hpp>
#include <ichor/stl/Common.h>
#include <iostream>

struct x {};
struct y {
    explicit y(const char *t) : z(t) {}
    std::string z;
};

TEST_CASE("unique_ptr") {

    SECTION("Ensure no leaks when run with ASAN") {
        auto rsrc = std::pmr::get_default_resource();
        auto p = Ichor::make_unique<x>(rsrc);

        auto p2 = Ichor::make_unique<y>(rsrc, "test");
        std::cout << p2->z << std::endl;
    }

}

TEST_CASE("polymorphic_allocator") {

    SECTION("Ensure no leaks when run with ASAN") {
        auto rsrc = std::pmr::get_default_resource();
        auto p = Ichor::make_unique<x>(rsrc);

        auto p2 = Ichor::make_unique<y>(rsrc, "test");
        std::cout << p2->z << std::endl;
    }

}