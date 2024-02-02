#include "Common.h"
#include <ichor/stl/RealtimeMutex.h>
#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <ichor/stl/ReferenceCountedPointer.h>
#include <ichor/stl/StringUtils.h>
#include <memory>
#include <string_view>
#include "TestServices/UselessService.h"

using namespace Ichor;
using namespace std::literals;

struct nonmoveable final {
    nonmoveable() = default;
    nonmoveable(const nonmoveable&) = default;
    nonmoveable(nonmoveable&&) = delete;
    nonmoveable& operator=(const nonmoveable&) = default;
    nonmoveable& operator=(nonmoveable&&) = delete;
};

struct nonmoveable_sbo_buster final {
    nonmoveable_sbo_buster() = default;
    nonmoveable_sbo_buster(const nonmoveable_sbo_buster&) = default;
    nonmoveable_sbo_buster(nonmoveable_sbo_buster&&) = delete;
    nonmoveable_sbo_buster& operator=(const nonmoveable_sbo_buster&) = default;
    nonmoveable_sbo_buster& operator=(nonmoveable_sbo_buster&&) = delete;

    std::array<std::byte, 32> buf;
};

struct noncopyable final {
    noncopyable() = default;
    noncopyable(const noncopyable&) = delete;
    noncopyable(noncopyable&&) = default;
    noncopyable& operator=(const noncopyable&) = delete;
    noncopyable& operator=(noncopyable&&) = default;
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

struct parent_test_class {

};

struct rc_test_class final : parent_test_class {
    rc_test_class(int _i, float _f, noncopyable) : i(_i), f(_f) {}
    rc_test_class(const rc_test_class&) = default;
    rc_test_class(rc_test_class&&) = delete;
    rc_test_class& operator=(const rc_test_class&) = default;
    rc_test_class& operator=(rc_test_class&&) = delete;

    int i;
    float f;
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


        const auto someNonmoveableSboBuster = make_unformattable_any<nonmoveable_sbo_buster>();
        REQUIRE_NOTHROW(any_cast<nonmoveable_sbo_buster>(someNonmoveableSboBuster));
        auto someMovedNonmoveableSboBuster = std::move(someNonmoveableSboBuster);
        auto someMoveConstructedNonmoveableSboBuster = Ichor::any(std::move(someNonmoveableSboBuster));

        auto emptyAny = Ichor::any();
        auto copyAssignedEmptyAny = emptyAny;
        auto copyConstructedEmptyAny = Ichor::any(emptyAny);
        auto moveAssignedEmptyAny = std::move(emptyAny);
        auto moveConstructedEmptyAny = Ichor::any(std::move(moveAssignedEmptyAny));

        someCopiedFromConstInt = std::move(someMovedNonMoveable);
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

    SECTION("ReferenceCountedPointer tests") {
        {
            ReferenceCountedPointer<int> i;
            REQUIRE(i.use_count() == 0);
            REQUIRE(!i.has_value());
            i = new int(5);
            REQUIRE(i.use_count() == 1);
            REQUIRE(*i == 5);
            REQUIRE(*i.get() == 5);
            REQUIRE(i.has_value());
        }
        {
            ReferenceCountedPointer<int> i{new int(6)};
            REQUIRE(i.use_count() == 1);
            REQUIRE(i.has_value());
            i = new int(5);
            REQUIRE(i.use_count() == 1);
            REQUIRE(i.has_value());
            REQUIRE(*i == 5);
            REQUIRE(*i.get() == 5);
        }
        bool deleted{};
        {
            auto deleter = [&deleted](int *ptr){ deleted = true; delete ptr; };
            std::unique_ptr<int, decltype(deleter)> unique(new int(5), deleter);
            ReferenceCountedPointer<int> tc{std::move(unique)};
            REQUIRE(tc.use_count() == 1);
            REQUIRE(*tc == 5);
            REQUIRE(!unique);
        }
        REQUIRE(deleted);
        deleted = false;
        {
            auto deleter = [&deleted](int *ptr){ deleted = true; delete ptr; };
            std::unique_ptr<int, decltype(deleter)> unique(new int(5), deleter);
            ReferenceCountedPointer<int> tc;
            tc = std::move(unique);
            REQUIRE(tc.use_count() == 1);
            REQUIRE(*tc == 5);
            REQUIRE(!unique);
        }
        REQUIRE(deleted);
        {
            ReferenceCountedPointer<int> tc = std::make_unique<int>(5);
            REQUIRE(tc.use_count() == 1);
            REQUIRE(*tc == 5);
        }
        {
            ReferenceCountedPointer<int> tc = Ichor::make_reference_counted<int>(5);
            REQUIRE(tc.use_count() == 1);
            REQUIRE(*tc == 5);
            tc = nullptr;
            REQUIRE(tc.use_count() == 0);
            REQUIRE(!tc.has_value());
        }
        {
            ReferenceCountedPointer<int> tc;
            REQUIRE(tc.use_count() == 0);
            tc = std::make_unique<int>(5);
            REQUIRE(tc.use_count() == 1);
            REQUIRE(*tc == 5);
        }
        {
            ReferenceCountedPointer<rc_test_class> tc = Ichor::make_reference_counted<rc_test_class>(5, 5.0f, noncopyable{});
            REQUIRE(tc.use_count() == 1);
            REQUIRE(tc->i == 5);
            REQUIRE(tc->f == 5.0f);
            ReferenceCountedPointer<parent_test_class> tc2{tc};
            REQUIRE(tc.use_count() == 2);
            REQUIRE(tc2.use_count() == 2);
            ReferenceCountedPointer<parent_test_class> tc3;
            REQUIRE(tc3.use_count() == 0);
            tc3 = tc;
            REQUIRE(tc.use_count() == 3);
            REQUIRE(tc2.use_count() == 3);
            REQUIRE(tc3.use_count() == 3);
        }
        {
            ReferenceCountedPointer<parent_test_class> tc{new rc_test_class(5, 5.0f, noncopyable{})};
            REQUIRE(tc.use_count() == 1);
            tc = Ichor::make_reference_counted<rc_test_class>(5, 5.0f, noncopyable{});
            REQUIRE(tc.use_count() == 1);
        }
        {
            ReferenceCountedPointer<parent_test_class> tc = Ichor::make_reference_counted<rc_test_class>(5, 5.0f, noncopyable{});
            REQUIRE(tc.use_count() == 1);
        }
        {
            ReferenceCountedPointer<int> i{new int(7)};
            ReferenceCountedPointer<int> i2{i};
            REQUIRE(i.use_count() == 2);
            REQUIRE(i2.use_count() == 2);
            REQUIRE(*i == 7);
            REQUIRE(*i2 == 7);
            i = new int(5);
            REQUIRE(i.use_count() == 1);
            REQUIRE(i2.use_count() == 1);
            REQUIRE(*i == 5);
            REQUIRE(*i2 == 7);
        }
        {
            ReferenceCountedPointer<int> i{new int(7)};
            ReferenceCountedPointer<int> i2;
            REQUIRE(i2.use_count() == 0);
            i2 = i;
            REQUIRE(i.use_count() == 2);
            REQUIRE(i2.use_count() == 2);
            REQUIRE(*i == 7);
            REQUIRE(*i2 == 7);
            i = new int(5);
            REQUIRE(i.use_count() == 1);
            REQUIRE(i2.use_count() == 1);
            REQUIRE(*i == 5);
            REQUIRE(*i2 == 7);
        }
        {
            ReferenceCountedPointer<int> i{new int(7)};
            ReferenceCountedPointer<int> i2{std::move(i)};
            REQUIRE(!i.has_value());
            REQUIRE(i2.has_value());
            REQUIRE(i.use_count() == 0);
            REQUIRE(i2.use_count() == 1);
            REQUIRE(*i2 == 7);
            i = new int(5);
            REQUIRE(i.has_value());
            REQUIRE(i2.has_value());
            REQUIRE(i.use_count() == 1);
            REQUIRE(i2.use_count() == 1);
            REQUIRE(*i == 5);
            REQUIRE(*i2 == 7);
        }
        {
            ReferenceCountedPointer<int> i{new int(7)};
            ReferenceCountedPointer<int> i2{};
            REQUIRE(i2.use_count() == 0);
            i2 = std::move(i);
            REQUIRE(!i.has_value());
            REQUIRE(i2.has_value());
            REQUIRE(i.use_count() == 0);
            REQUIRE(i2.use_count() == 1);
            REQUIRE(*i2 == 7);
            i = new int(5);
            REQUIRE(i.has_value());
            REQUIRE(i2.has_value());
            REQUIRE(i.use_count() == 1);
            REQUIRE(i2.use_count() == 1);
            REQUIRE(*i == 5);
            REQUIRE(*i2 == 7);
        }
    }

    SECTION("FastAtoi(u) tests") {
        REQUIRE(Ichor::FastAtoiu("10") == 10);
        REQUIRE(Ichor::FastAtoiu("0") == 0);
        REQUIRE(Ichor::FastAtoiu("u10") == 0);
        REQUIRE(Ichor::FastAtoiu("10u") == 10);
        REQUIRE(Ichor::FastAtoiu(std::to_string(std::numeric_limits<uint64_t>::max()).c_str()) == std::numeric_limits<uint64_t>::max());
        REQUIRE(Ichor::FastAtoi("10") == 10);
        REQUIRE(Ichor::FastAtoi("0") == 0);
        REQUIRE(Ichor::FastAtoi("u10") == 0);
        REQUIRE(Ichor::FastAtoi("10u") == 10);
        REQUIRE(Ichor::FastAtoi("-10") == -10);
        REQUIRE(Ichor::FastAtoi(std::to_string(std::numeric_limits<int64_t>::max()).c_str()) == std::numeric_limits<int64_t>::max());
        REQUIRE(Ichor::FastAtoi(std::to_string(std::numeric_limits<int64_t>::min()).c_str()) == std::numeric_limits<int64_t>::min());
    }

    SECTION("string_view split tests") {
        std::string_view s{"this\nis\na\nstring"};
        auto ret = split(s, "\n");
        REQUIRE(ret.size() == 4);
        ret = split(s, " ");
        REQUIRE(ret.size() == 1);
    }

    SECTION("Version parse tests") {
        REQUIRE(!parseStringAsVersion("").has_value());
        REQUIRE(!parseStringAsVersion("0").has_value());
        REQUIRE(!parseStringAsVersion("0.0").has_value());
        REQUIRE(!parseStringAsVersion("-1.2.3").has_value());
        REQUIRE(!parseStringAsVersion("a.b.c").has_value());
        REQUIRE(!parseStringAsVersion("1.2.3.4").has_value());
        REQUIRE(!parseStringAsVersion("1.2.3.").has_value());

        auto v = parseStringAsVersion("0.0.0");
        REQUIRE(v.has_value());
        REQUIRE((*v).major == 0);
        REQUIRE((*v).minor == 0);
        REQUIRE((*v).patch == 0);

        v = parseStringAsVersion("27398.128937618973.123123");
        REQUIRE(v.has_value());
        REQUIRE((*v).major == 27398);
        REQUIRE((*v).minor == 128937618973);
        REQUIRE((*v).patch == 123123);
    }

    SECTION("Version compare tests") {
        Version v1{1, 2, 3};
        REQUIRE(Version{1, 2, 4} > v1);
        REQUIRE(Version{1, 3, 3} > v1);
        REQUIRE(Version{2, 2, 3} > v1);
        REQUIRE(Version{2, 0, 0} > v1);
        REQUIRE(Version{1, 3, 0} > v1);
        REQUIRE(!(Version{1, 2, 3} > v1));
        REQUIRE(Version{1, 2, 3} >= v1);
        REQUIRE(!(Version{1, 2, 2} >= v1));
        REQUIRE(v1 > Version{1, 2, 2});
        REQUIRE(!(v1 > Version{1, 2, 3}));
        REQUIRE(v1 >= Version{1, 2, 3});
        REQUIRE(!(v1 >= Version{1, 2, 4}));
        REQUIRE(v1 < Version{1, 2, 4});
        REQUIRE(!(v1 < Version{1, 2, 3}));
        REQUIRE(v1 <= Version{1, 2, 3});
        REQUIRE(!(v1 <= Version{1, 2, 2}));
        REQUIRE(v1 == Version{1, 2, 3});
        REQUIRE(v1 != Version{2, 2, 3});
        REQUIRE(v1 != Version{1, 3, 3});
        REQUIRE(v1 != Version{1, 2, 4});
    }

    SECTION("Basename tests") {
        REQUIRE(Ichor::basename("") == ""sv);
        REQUIRE(Ichor::basename("file.cpp") == "file.cpp"sv);
#ifdef _WIN32
        REQUIRE(Ichor::basename("path\\file.cpp") == "file.cpp"sv);
#else
        REQUIRE(Ichor::basename("path/file.cpp") == "file.cpp"sv);
        REQUIRE(Ichor::basename("more/path/file.cpp") == "file.cpp"sv);
#endif
    }
}
