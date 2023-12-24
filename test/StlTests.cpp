#include "Common.h"
#include <ichor/stl/RealtimeMutex.h>
#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include "TestServices/UselessService.h"

using namespace Ichor;

struct nonmoveable {
    nonmoveable() = default;
    nonmoveable(const nonmoveable&) = default;
    nonmoveable(nonmoveable&&) = delete;
    nonmoveable& operator=(const nonmoveable&) = default;
    nonmoveable& operator=(nonmoveable&&) = delete;
};

template <>
struct fmt::formatter<nonmoveable> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const nonmoveable& change, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "nonmoveable");
    }
};

TEST_CASE("STL Tests") {

    SECTION("Any basics") {
        auto someInt = make_any<uint64_t>(5u);
        REQUIRE_NOTHROW(any_cast<uint64_t>(someInt));
        REQUIRE(any_cast<uint64_t>(someInt) == 5u);
        REQUIRE(someInt.to_string() == "5");
        REQUIRE(someInt.get_size() == sizeof(uint64_t));
        REQUIRE(someInt.type_hash() == typeNameHash<uint64_t>());
        REQUIRE(someInt.type_name() == typeName<uint64_t>());
        REQUIRE_THROWS_MATCHES(any_cast<float>(someInt), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<int64_t>(someInt), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<int32_t>(someInt), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<uint32_t>(someInt), bad_any_cast, ExceptionMatcher());

        any_cast<uint64_t&>(someInt) = 10;
        REQUIRE(any_cast<uint64_t>(someInt) == 10u);

        auto someString = make_any<std::string>("test");
        REQUIRE_NOTHROW(any_cast<std::string>(someString));
        REQUIRE(any_cast<std::string>(someString) == "test");
        REQUIRE(someString.to_string() == "test");
        REQUIRE(someString.get_size() == sizeof(std::string));
        REQUIRE(someString.type_hash() == typeNameHash<std::string>());
        REQUIRE(someString.type_name() == typeName<std::string>());
        REQUIRE_THROWS_MATCHES(any_cast<float>(someString), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<int64_t>(someString), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<int32_t>(someString), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<uint32_t>(someString), bad_any_cast, ExceptionMatcher());

        someString = make_any<float>(4.5f);
        REQUIRE_NOTHROW(any_cast<float>(someString));
        REQUIRE(any_cast<float>(someString) == 4.5f);
        REQUIRE_THROWS_MATCHES(any_cast<std::string>(someString), bad_any_cast, ExceptionMatcher());

        auto someMovedFloat{std::move(someString)};
        REQUIRE_NOTHROW(any_cast<float>(someMovedFloat));
        REQUIRE(any_cast<float>(someMovedFloat) == 4.5f);
        REQUIRE_THROWS_MATCHES(any_cast<float>(someString), bad_any_cast, ExceptionMatcher());
        REQUIRE(someMovedFloat.to_string() == "4.5");
        REQUIRE(someString.to_string() == "Unprintable value");

        auto someCopiedInt = someInt;
        REQUIRE_NOTHROW(any_cast<uint64_t>(someCopiedInt));
        REQUIRE(any_cast<uint64_t>(someCopiedInt) == 10u);
        REQUIRE(any_cast<uint64_t>(someInt) == 10u);
        REQUIRE(someInt.to_string() == "10");
        REQUIRE(someCopiedInt.to_string() == "10");

        auto someMovedInt = std::move(someInt);
        REQUIRE_NOTHROW(any_cast<uint64_t>(someMovedInt));
        REQUIRE(any_cast<uint64_t>(someMovedInt) == 10u);
        REQUIRE(someMovedInt.to_string() == "10");
        REQUIRE(someInt.to_string() == "Unprintable value");
        REQUIRE_THROWS_MATCHES(any_cast<uint64_t>(someInt), bad_any_cast, ExceptionMatcher());

        const auto someConstInt = make_any<int>(12);
        REQUIRE_NOTHROW(any_cast<int>(someConstInt));
        REQUIRE(any_cast<int>(someConstInt) == 12);

        auto someCopiedFromConstInt{someConstInt};
        REQUIRE_NOTHROW(any_cast<int>(someConstInt));
        REQUIRE(any_cast<int>(someConstInt) == 12);
        REQUIRE_NOTHROW(any_cast<int>(someCopiedFromConstInt));
        REQUIRE(any_cast<int>(someCopiedFromConstInt) == 12);

        const auto someNonmoveable = make_any<nonmoveable>();
        REQUIRE_NOTHROW(any_cast<nonmoveable>(someNonmoveable));
        REQUIRE(someNonmoveable.to_string() == "nonmoveable");
        auto someMovedNonMoveable = std::move(someNonmoveable);
        REQUIRE(someMovedNonMoveable.to_string() == "nonmoveable");

        any noneAny;
        REQUIRE_THROWS_MATCHES(any_cast<float>(noneAny), bad_any_cast, ExceptionMatcher());
        REQUIRE(noneAny.to_string() == "Unprintable value");

        auto movedNoneAny = std::move(noneAny);
        REQUIRE_THROWS_MATCHES(any_cast<float>(noneAny), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<float>(movedNoneAny), bad_any_cast, ExceptionMatcher());
        REQUIRE(movedNoneAny.to_string() == "Unprintable value");
        REQUIRE(noneAny.to_string() == "Unprintable value");

    }

    SECTION("RealTimeMutex basics") {
        RealtimeMutex m;
        m.lock();

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        // critical section in windows is recursive
        REQUIRE(m.try_lock() == true);
#else
        REQUIRE(m.try_lock() == false);
#endif

        m.unlock();

        REQUIRE(m.try_lock() == true);

        m.unlock();
    }

    SECTION("RealtimeReadWriteMutex basics") {
        RealtimeReadWriteMutex m;
        m.lock();

        REQUIRE(m.try_lock() == false);
        REQUIRE(m.try_lock_shared() == false);

        m.unlock();

        REQUIRE(m.try_lock() == true);
        REQUIRE(m.try_lock_shared() == false);

        m.unlock();

        REQUIRE(m.try_lock_shared() == true);
        REQUIRE(m.try_lock_shared() == true);
        REQUIRE(m.try_lock() == false);

        m.unlock_shared();
        m.unlock_shared();
    }

    SECTION("typeName tests") {
        REQUIRE(typeName<UselessService>() == typeName<Ichor::UselessService>());
        REQUIRE(typeNameHash<UselessService>() == typeNameHash<Ichor::UselessService>());
    }

    SECTION("NeverNull tests") {
        NeverNull<int*> p = new int(120);
        auto f = [](NeverNull<int*> p2) { *p2 += 1; };
        REQUIRE(*p == 120);
        f(p);
        REQUIRE(*p == 121);
        delete p;
    }
}
