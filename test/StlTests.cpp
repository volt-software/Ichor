#include "Common.h"
#include <ichor/stl/RealtimeMutex.h>
#include <ichor/stl/RealtimeReadWriteMutex.h>
#include "TestServices/UselessService.h"

using namespace Ichor;

TEST_CASE("STL Tests") {

    SECTION("Any basics") {
        auto someInt = make_any<uint64_t>(5);
        REQUIRE_NOTHROW(any_cast<uint64_t>(someInt));
        REQUIRE(any_cast<uint64_t>(someInt) == 5u);
        REQUIRE_THROWS_MATCHES(any_cast<float>(someInt), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<int64_t>(someInt), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<int32_t>(someInt), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<uint32_t>(someInt), bad_any_cast, ExceptionMatcher());

        any_cast<uint64_t&>(someInt) = 10;
        REQUIRE(any_cast<uint64_t>(someInt) == 10u);

        auto someString = make_any<std::string>("test");
        REQUIRE_NOTHROW(any_cast<std::string>(someString));
        REQUIRE(any_cast<std::string>(someString) == "test");
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

        auto someCopiedInt = someInt;
        REQUIRE_NOTHROW(any_cast<uint64_t>(someCopiedInt));
        REQUIRE(any_cast<uint64_t>(someCopiedInt) == 10u);
        REQUIRE(any_cast<uint64_t>(someInt) == 10u);

        auto someMovedInt = std::move(someInt);
        REQUIRE_NOTHROW(any_cast<uint64_t>(someMovedInt));
        REQUIRE(any_cast<uint64_t>(someMovedInt) == 10u);
        REQUIRE_THROWS_MATCHES(any_cast<uint64_t>(someInt), bad_any_cast, ExceptionMatcher());

        const auto someConstInt = make_any<int>(12);
        REQUIRE_NOTHROW(any_cast<int>(someConstInt));
        REQUIRE(any_cast<int>(someConstInt) == 12);

        auto someCopiedFromConstInt{someConstInt};
        REQUIRE_NOTHROW(any_cast<int>(someConstInt));
        REQUIRE(any_cast<int>(someConstInt) == 12);
        REQUIRE_NOTHROW(any_cast<int>(someCopiedFromConstInt));
        REQUIRE(any_cast<int>(someCopiedFromConstInt) == 12);

        any noneAny;
        REQUIRE_THROWS_MATCHES(any_cast<float>(noneAny), bad_any_cast, ExceptionMatcher());

        auto movedNoneAny = std::move(noneAny);
        REQUIRE_THROWS_MATCHES(any_cast<float>(noneAny), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<float>(movedNoneAny), bad_any_cast, ExceptionMatcher());

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
}